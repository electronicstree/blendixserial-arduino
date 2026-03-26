/*
  Arduino-to-Blender | HC‑SR04 Ultrasonic Distance Sensor

  Author   : Usman
  Date     : 25 March 2026
  Website  : www.electronicstree.com
  Email    : help@electronicstree.com

  Description
  -----------
  This sketch reads distance from an HC‑SR04 ultrasonic sensor and sends the
  measured value (in cm) to Blender as the Y‑coordinate of object 0. This can
  be used to drive a 3D object (e.g., a floating ball, a bar graph, or a character)
  that moves up/down in response to real‑world distances.

  Only when the distance changes (rounded to the nearest cm) a new packet is sent,
  preventing unnecessary serial traffic.


  Wiring
  ------
  HC‑SR04:
      VCC → 5V
      GND → GND
      TRIG → pin 9
      ECHO → pin 10

  Arduino:
      USB → PC (for Blender communication)


  Setup in Blender
  ----------------
  1. Install the blendixserial add‑on.
  2. Set the COM port and baud rate (38400 – change in sketch if needed).
  3. Choose "Receive Data".
  4. Assign an object to Object ID 0.
  5. Enable "Location" for that object.
  6. Start the connection – the object's Y position will follow the distance.
*/

#include "blendixserial.h"

// Sensor pins
const int TRIG_PIN = 9;
const int ECHO_PIN = 10;

// Blendix objects
blendixserial blendix;
uint8_t buf[BLENDIXSERIAL_MAX_PACKET_SIZE];

// Timing
unsigned long lastRead = 0;
const unsigned long READ_INTERVAL_MS = 100;  // read every 100 ms (10 Hz)

float lastDistance = -1.0;   // last sent distance (cm)

void setup() {
  Serial.begin(38400);       // Must match Blender baud rate
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}

void loop() {
  // Read distance at fixed intervals (non‑blocking)
  if (millis() - lastRead >= READ_INTERVAL_MS) {
    lastRead = millis();

    // 1. Measure distance
    float distance = measureDistanceCM();

    // 2. Round to nearest integer cm to avoid sending tiny changes
    float rounded = round(distance);

    // 3. Send only if changed
    if (rounded != lastDistance) {
      // Set Y location of object 0 to the distance
      blendix.setLocation(0, 0.0, rounded, 0.0);

      uint16_t len = blendix.bodBuild(buf);
      if (len > 0) {
        Serial.write(buf, len);
      }

      lastDistance = rounded;
    }
  }
}

// --------------------------------------------------------------------
// HC‑SR04 distance measurement (returns distance in centimeters)
// --------------------------------------------------------------------
float measureDistanceCM() {
  // Send a 10 µs pulse to trigger the sensor
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Measure the echo pulse duration (in microseconds)
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);  // timeout after 30 ms (about 5 m)

  // If no echo within timeout, return a large number (e.g., 999)
  if (duration == 0) return 999.0;

  // Speed of sound = 343 m/s = 0.0343 cm/µs
  // Distance = (duration / 2) * speed of sound
  float distance = (duration / 2.0) * 0.0343;
  return distance;
}
