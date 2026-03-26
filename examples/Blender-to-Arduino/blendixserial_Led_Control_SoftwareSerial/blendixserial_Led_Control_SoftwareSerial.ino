/*
  Blender-to-Arduino |  Led Control Example Sketch with SoftwareSerial 

  Author: Usman 
  Date: 10-MAR-2026
  Website: www.electronicstree.com
  Email: help@electronicstree.com

  Description
  -----------
  This example demonstrates how to use the blendixserial library with
  SoftwareSerial to receive object location data from Blender and control
  hardware connected to the Arduino.

  Blender sends the 3D position (Z) of Object 0 through a serial
  connection. The Arduino reads the incoming data and monitors the Z value.

  When the Z value changes:
      Z = 0.00   -> LED OFF
      Z = -0.15  -> LED ON

  The LED used in this example is connected to pin 4.



  Why SoftwareSerial is Used
  --------------------------
  The Arduino hardware serial port (Serial) is also used by the USB
  connection for uploading sketches and for debugging with the Serial
  Monitor. Because of this, it can only be used by one program at a time.

  By using SoftwareSerial, Blender communication is moved to different
  digital pins instead of the main hardware serial port. This allows the
  main Serial port to remain available for debugging and monitoring.

  In this example:
      Arduino Pin 10 -> RX (receives data from Blender)
      Arduino Pin 11 -> TX (sends data to Blender)

  Important:
  You **cannot connect these Arduino pins directly to a USB port**. 
  To communicate with Blender via SoftwareSerial, you must use a
  **TTL-to-USB converter** (also called an FTDI adapter or USB-to-serial
  module). This safely converts the 5V TTL signals from the Arduino pins
  to proper USB levels for the computer.

  The main Serial port can still be used for debugging through the
  Arduino Serial Monitor while Blender communicates through SoftwareSerial.



  Wiring
  ------
  TTL-to-USB converter TX -> Arduino Pin 10 (SoftwareSerial RX)
  TTL-to-USB converter RX -> Arduino Pin 11 (SoftwareSerial TX)

*/

#include <SoftwareSerial.h>  
#include "blendixserial.h"    

// Define pins used for SoftwareSerial communication with Blender
#define BLENDER_RX 10
#define BLENDER_TX 11

// Define LED output pin
#define LED_PIN 4

// Create a SoftwareSerial object for communication with Blender
SoftwareSerial blenderSerial(BLENDER_RX, BLENDER_TX); 

// Create an instance of the Blendix parser
blendixserial blendix;

// Variable to store the last Z value received
// Initialized with an impossible value to ensure first update triggers
float lastZ = -999;

void setup()
{
    // Start hardware serial for debugging (Arduino ↔ PC)
    Serial.begin(9600);

    // Start software serial for communication with Blender
    blenderSerial.begin(9600);

    // Set LED pin as output
    pinMode(LED_PIN, OUTPUT);

    // Turn LED OFF initially
    digitalWrite(LED_PIN, LOW);
    
    // Print message to indicate Arduino is ready
    Serial.println("Arduino Ready");
}

void loop()
{
    // Check if data is available from Blender
    while (blenderSerial.available())
    {
        // Read one byte from Blender
        uint8_t byteIn = blenderSerial.read();

        // Feed byte into the Blendix parser
        // When a full valid packet is received, bodParse returns true
        if (blendix.bodParse(byteIn))
        {
            // Read object 0 position data
            readObject0();
        }
    }
}

// Function to read location of object 0 from Blender
void readObject0()
{
    float x, y, z;

    // Get the 3D position (x, y, z) of object index 0
    blendix.getLocation(0, x, y, z);
    
    // Check if the Z value has changed since the last update
    if (z != lastZ)
    {
        // Print Z value to Serial Monitor
        Serial.print("Z: ");
        Serial.println(z);
        
        // Control LED depending on the Z value
        
        // If object Z position is exactly 0.00 → turn LED OFF
        if (z == 0.00)
        {
            digitalWrite(LED_PIN, LOW);
            Serial.println("LED OFF");
        }

        // If object Z position is -0.15 → turn LED ON
        else if (z == -0.15)
        {
            digitalWrite(LED_PIN, HIGH);
            Serial.println("LED ON");
        }
        
        // Update lastZ to current value
        lastZ = z;
    }
}