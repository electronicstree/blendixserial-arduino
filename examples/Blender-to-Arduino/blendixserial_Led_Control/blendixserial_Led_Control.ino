/*

  Blender-to-Arduino | Led Control Example Sketch

  Author: Usman 
  Date: 10-MAR-2026
  Website: www.electronicstree.com
  Email: help@electronicstree.com

  Description
  -----------
  This example demonstrates how to use the blendixserial library to receive
  object location data from Blender and use it to control hardware on Arduino.

  Blender sends the 3D position (X, Y, Z) of Object 0 through the serial
  connection. The Arduino reads the incoming data and monitors the Z value.

  When the Z value changes:
      Z = 0.00   -> LED OFF
      Z = -0.15  -> LED ON

  The LED used in this example is connected to pin 4.

  This shows how Blender can interact with physical hardware in real time
  using serial communication.



  Serial Communication
  --------------------
  This example uses the Arduino hardware serial port (Serial) over USB.
  Blender sends data to Arduino through the same USB cable used to upload
  the sketch.

  Important: Only ONE program can use a serial port at a time.

  Correct workflow:
  1. Upload the sketch to Arduino.
  2. Close the Arduino Serial Monitor if it is open.
  3. Run the blendixsrial-addon that sends the object data.

  If the Serial Monitor or another serial program is open, Blender will
  not be able to connect to the Arduino because the serial port is already in use.
*/


#include "blendixserial.h"

// LED output pin
#define LED_PIN 4

// Create Blendix parser object
blendixserial blendix;

// Store last received Z value
float lastZ = -999;

void setup()
{
    // Start hardware serial (used for Blender communication)
    Serial.begin(9600);

    // Configure LED pin
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
}

void loop()
{
    // Read incoming bytes from Blender
    while (Serial.available())
    {
        uint8_t byteIn = Serial.read();

        // Parse incoming Blendix data packet
        if (blendix.bodParse(byteIn))
        {
            readObject0();
        }
    }
}

// Read position of object 0
void readObject0()
{
    float x, y, z;

    // Get object location from Blendix
    blendix.getLocation(0, x, y, z);

    // Only react if Z value changed
    if (z != lastZ)
    {
        // Control LED based on Z value
        if (z == 0.00)
        {
            digitalWrite(LED_PIN, LOW);
        }
        else if (z == -0.15)
        {
            digitalWrite(LED_PIN, HIGH);
        }

        // Save last Z value
        lastZ = z;
    }
}