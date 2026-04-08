/**
  Arduino ↔ Blender | Bidirectional communication example 

  Author   : Usman
  Date     : 9 April 2026
  Website  : www.electronicstree.com
  Email    : help@electronicstree.com


 What this sketch does
 ─────────────────────
 RECEIVING  (Blender → Arduino)
   Reads the Location, Rotation and Scale of three objects sent by the
   Blender add-on in real time.  Each received value drives a real output:

     Object 0 Location X  →  fades the built-in LED (PWM on pin 9)
     Object 0 Rotation Z  →  printed to Serial Monitor as a degree value
     Object 1 Scale    X  →  printed to Serial Monitor as a scale factor

 SENDING  (Arduino → Blender)
   Every 50 ms the sketch sends back:
     Object 0  Location  —  X/Y driven by a potentiometer (A0)
                            Z driven by a push-button (D4)
     Object 1  Rotation Z  —  a smooth sine wave so you can see animation
     Text message          —  "OK" while button is released, "BTN" while held

 Hardware needed (all optional — sketch still compiles/runs without them)
  ─────────────────────────────────────────────────────────────────────────
   Pin  9  (PWM)    — LED + 220 Ω resistor to GND  (brightness from Blender)
   Pin  4  (INPUT)  — Momentary push-button to GND  (internal pull-up used)
   Pin A0  (ADC)    — 10 kΩ potentiometer wiper     (mapped to X location)

 Board compatibility
 ───────────────────
   Tested:   Arduino Uno R3, Arduino Mega 2560
   Expected: Any board supported by the Arduino framework —
             Nano, Leonardo, ESP8266, ESP32 (using Serial0 by default)

   For ESP8266 / ESP32 replace "Serial" with "Serial1" or a SoftwareSerial
   instance if you need the USB port free for the IDE monitor.

 Baud rate
 ─────────
   115200 baud — must match the blendixserial Blender add-on setting.

 Author: Usman
 Maintainer: https://github.com/ELECTRONICSTREE/BlendixSerial-Arduino

 */


// Includes 
#include <blendixserial.h>
#include <SoftwareSerial.h>   
#include <Servo.h> 

//Pin definitions
static const uint8_t PIN_LED    = 9;   // PWM-capable pin — brightness from Blender
static const uint8_t PIN_BUTTON = 4;   // active-LOW push-button (internal pull-up)
static const uint8_t PIN_POT    = A0;  // potentiometer for X location

SoftwareSerial debugSerial(10, 11);


// Object index constants 
// Give your objects readable names so the rest of the sketch is self-documenting.
// These must match the object indices configured in the Blender add-on.
static const uint8_t OBJ_CUBE   = 0;   // Main cube — full transform sent & received
static const uint8_t OBJ_SPHERE = 1;   // Sphere    — rotation Z sent, scale X received


// Timing 
static const uint32_t SEND_INTERVAL_MS = 50;   // transmit rate: 20 Hz


// Global objects 
blendixserial protocol;  // the protocol handler
uint8_t txBuffer[BLENDIXSERIAL_MAX_PACKET_SIZE]; // reusable TX frame buffer


// Forward declarations 
void handleReceivedData();
void sendData();


void setup()
{

    Serial.begin(115200);
    debugSerial.begin(9600);

    // Hardware I/O
    pinMode(PIN_LED,    OUTPUT);
    pinMode(PIN_BUTTON, INPUT_PULLUP);  // button reads LOW when pressed
}


void loop()
{
    // 1. Feed every available byte into the parser 
    // bodParse() is a one-byte-at-a-time FSM.  It returns true exactly once per
    // complete, checksum-valid packet.  We call handleReceivedData() at that
    // moment so the received values are always acted on in the same loop tick
    // that they arrive — no extra latency.

    while (Serial.available())
    {
        if (protocol.bodParse(Serial.read()))
        {
            handleReceivedData();
        }
    }

    // 2. Send our data at a fixed rate
    // millis()-based timing keeps the send rate independent of loop() speed.
    // We do NOT call delay() anywhere — that would block the RX path above.
    static uint32_t lastSendMs = 0;
    uint32_t now = millis();

    if (now - lastSendMs >= SEND_INTERVAL_MS)
    {
        lastSendMs = now;
        sendData();
    }
}



// handleReceivedData()
// Called once per valid incoming packet.
// Reads the freshly updated values out of the protocol object and drives
// hardware / prints diagnostics.

void handleReceivedData()
{
    // Object 0 (Cube) — Location X drives LED brightness
    // Blender Location X is in metres, typically ranging from roughly -10 to +10
    // depending on your scene scale.  We map it to a PWM duty cycle (0–255).
    // axisAvailable() guards against acting on uninitialised data.
    if (protocol.axisAvailable(OBJ_CUBE, Location, X))
    {
        float locX = protocol.getValue(OBJ_CUBE, Location, X);

        // Map -5.0 .. +5.0 metres to 0 .. 255 PWM.
        // constrain() clamps the result so out-of-range scene values
        // never produce an invalid analogWrite argument.
        int brightness = (int)((locX + 5.0f) * 25.5f);
        brightness = constrain(brightness, 0, 255);
        analogWrite(PIN_LED, brightness);
    }

    // Object 0 (Cube) — Rotation Z printed to Serial Monitor
    // Blender sends rotation in degrees.
    // We only print when the axis is available to avoid flooding the monitor
    // with zeros before Blender has sent anything.
    if (protocol.axisAvailable(OBJ_CUBE, Rotation, Z))
    {
        float rotZ_deg = protocol.getValue(OBJ_CUBE, Rotation, Z);

        debugSerial.print(F("Cube RotZ: "));
        debugSerial.print(rotZ_deg, 1);
        debugSerial.println(F(" deg"));
    }

    // Object 1 (Sphere)  Scale X printed to Serial Monitor 
    if (protocol.axisAvailable(OBJ_SPHERE, Scale, X))
    {
        float scaleX = protocol.getValue(OBJ_SPHERE, Scale, X);

        debugSerial.print(F("Sphere ScaleX: "));
        debugSerial.println(scaleX, 3);
    }

    //  Convenience: read the full Location triplet at once
    // getLocation() fills three float references in one call.
    // It does not check axisAvailable() internally — uninitialised axes
    // return 0.0f.  Use it when you know Blender is sending all three axes.

    float lx, ly, lz;
    protocol.getLocation(OBJ_CUBE, lx, ly, lz);
    // (Use lx, ly, lz here if needed — omitting print to keep monitor clean)
}



// sendData()
// Reads hardware inputs, populates the protocol object with new values,
// then calls bodBuild() to assemble and transmit a frame.
// Delta compression: only axes explicitly set since the last successful
// bodBuild() call are transmitted.  If none of the inputs changed you can
// still call set*() and bodBuild() — the bitmask will include those axes
// and Blender receives the latest value.  If you want true "send only on
// change" behaviour, compare to previous values before calling setValue().

void sendData()
{

    // Potentiometer (A0): 0 – 1023 → mapped to -5.0 .. +5.0 Blender units
    int   rawPot = analogRead(PIN_POT);
    float potX   = ((float)rawPot / 511.5f) - 1.0f;   // -1.0 .. +1.0
    potX *= 5.0f;                                       // -5.0 .. +5.0

    // Push-button (D4): HIGH = released, LOW = pressed (active-LOW + pull-up)
    bool buttonPressed = (digitalRead(PIN_BUTTON) == LOW);

    // Smooth aniamton sphere rotation 
    static float angle = 0.0f;
    angle += 0.01f; // slower for smooth back-and-forth
    if (angle > 1.0f) angle -= 1.0f;

    // Map 0..1 to -180..180 degrees
    float sineRotDeg = (angle < 0.5f ? angle * 4.0f - 1.0f : 3.0f - angle * 4.0f) * 180.0f;


    //  Set values on the protocol object ─
    // You can use individual setValue() calls for fine control:
    //   protocol.setValue(OBJ_CUBE, Location, X, potX);
    // Or the convenience wrappers that set all three axes at once:
    //   protocol.setLocation(OBJ_CUBE, locX, locY, locZ);
    // Axes not set here are simply not included in the outgoing packet —
    // Blender will keep whatever values those objects had previously.

    // Object 0 (Cube): send Location X from pot, Y fixed, Z from button
    protocol.setValue(OBJ_CUBE, Location, X,  potX);
    protocol.setValue(OBJ_CUBE, Location, Y,  0.0f);
    protocol.setValue(OBJ_CUBE, Location, Z,  buttonPressed ? 2.0f : 0.0f);

    // Object 1 (Sphere): send only Rotation Z (the sine wave)
    protocol.setValue(OBJ_SPHERE, Rotation, Z, sineRotDeg);

    // Text: "BTN" while button held, "OK" otherwise
    // setText() queues the string — it will be included in the next packet
    // as msgType 3 (objects + text) automatically.
    protocol.setText(buttonPressed ? "BTN" : "OK");


    // Build and transmit the frame 
    // bodBuild() serialises all dirty values into txBuffer and returns the
    // total byte count.  It returns 0 if nothing is pending or the payload
    // would overflow MAX_PAYLOAD (dirty flags are preserved in that case).
    // Serial.write(buffer, len) sends the raw binary frame.
    // Do NOT use Serial.print() for protocol data — it would corrupt the frame.

    uint16_t frameLen = protocol.bodBuild(txBuffer);

    if (frameLen > 0)
    {
        Serial.write(txBuffer, frameLen);
    }
}
