/*
  Blender-to-Arduino | Servo Control Example

  Author   : Usman
  Date     : 25 March 2026
  Website  : www.electronicstree.com
  Email    : help@electronicstree.com

  Description
  -----------
  This example demonstrates how to use the BlendixSerial library to
  receive object rotation data from Blender and control a servo motor
  connected to an Arduino.

  Blender sends the Z‑rotation of Object 0 over the USB connection.
  The Arduino reads the incoming value and maps it to the servo angle:
      Rotation Z =   0°   →  Servo at   0°
      Rotation Z = 180°   →  Servo at 180°

  The servo signal wire is connected to pin 9.

 
  Why SoftwareSerial is Used
  --------------------------
  The Arduino’s hardware serial port (Serial) is directly connected to
  the USB port through an onboard converter. Using it for Blender
  communication allows a simple USB cable connection without any
  additional hardware.

  Debugging is moved to a SoftwareSerial port on two digital pins.
  This keeps the main USB port dedicated to Blender while still
  allowing serial debug output to be viewed on a second Serial Monitor
  (via a TTL‑to‑USB converter).

  In this example:
      Arduino Pin 10 → TX (debug output)
      Arduino Pin 11 → RX (unused, but reserved)

  Important: To view debug messages, connect a **TTL‑to‑USB converter**
  to pins 10 (TX) and GND, and open the corresponding COM port in the
  Serial Monitor. The main USB cable remains connected to Blender.


  Wiring
  ------

  | Arduino (USB)      | Blender (PC)       |
  |--------------------|--------------------|
  | USB port           | USB port           |
  | (hardware serial)  | (direct connection)|

  | TTL‑to‑USB Converter | Arduino           |
  |----------------------|-------------------|
  | RX                   | Pin 10 (TX debug) |
  | GND                  | GND               |
  | (optional TX)        | Pin 11 (RX)       |

  | Servo                | Arduino           |
  |----------------------|-------------------|
  | Signal               | Pin 9             |
  | VCC                  | 5V                |
  | GND                  | GND               |


  Note
  ----
  If you do not need debugging, you can omit the SoftwareSerial setup.
  The main Blender communication will still work through the USB cable.
*/

#include "blendixserial.h"      
#include <SoftwareSerial.h>     
#include <Servo.h>             

blendixserial blendix;          // Instance to parse Blender data

// SoftwareSerial for debugging: RX = 10 (unused), TX = 11
SoftwareSerial debugSerial(10, 11);

Servo myServo;                  // Servo object

int lastServoPos = -1;          // Stores last servo angle to avoid repeated writes

void setup()
{
  // Hardware serial (USB) receives data from Blender
  Serial.begin(9600);

  // Software serial for debug messages (view on a separate USB‑TTL adapter)
  debugSerial.begin(9600);

  // Attach the servo to pin 9
  myServo.attach(9);

  debugSerial.println("Servo test start");   // Indicate setup is complete
}

void loop()
{
  // Read all available bytes from the hardware serial (Blender data)
  while (Serial.available())
  {
    // Feed each byte to the Blendix parser; if a complete frame is received...
    if (blendix.bodParse(Serial.read()))
    {
      // ...update the servo position
      updateServo();
    }
  }
}

// Function to read the latest rotation data and move the servo accordingly
void updateServo()
{
  float x, y, z;   // Variables to hold rotation angles

  // Check if Z‑rotation data for object 0 is available
  if (blendix.axisAvailable(0, Rotation, Z))
  {
    // Retrieve the full rotation triplet for object 0
    blendix.getRotation(0, x, y, z);

    // Convert Z rotation (expected range 0–180) to servo angle,
    // then constrain to valid servo range (0–180)
    int servoPos = constrain((int)z, 0, 180);

    // Only move the servo and print debug if the angle actually changed
    if (servoPos != lastServoPos)
    {
      myServo.write(servoPos);          // Command the servo to the new angle
      lastServoPos = servoPos;          // Update last stored position

      // Print debug information via SoftwareSerial
      debugSerial.print("RotZ: ");
      debugSerial.print(z);
      debugSerial.print("  Servo: ");
      debugSerial.println(servoPos);
    }
  }
}