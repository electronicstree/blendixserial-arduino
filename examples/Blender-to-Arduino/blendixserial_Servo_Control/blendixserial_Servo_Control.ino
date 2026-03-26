/*
  Blender-to-Arduino | Servo Control Example

  Author   : Usman
  Date     : 25 March 2026
  Website  : www.electronicstree.com
  Email    : help@electronicstree.com

  Description
  -----------
  This minimal example demonstrates how to use the BlendixSerial library
  to receive object rotation data from Blender and control a servo motor
  connected to an Arduino.

  Blender sends the Z‑rotation of Object 0 over the USB connection.
  The Arduino reads the incoming value and maps it to the servo angle:
      Rotation Z =   0°   →  Servo at   0°
      Rotation Z = 180°   →  Servo at 180°

  The servo signal wire is connected to pin 9.


  Wiring
  ------

  | Servo                | Arduino           |
  |----------------------|-------------------|
  | Signal               | Pin 9             |
  | VCC                  | 5V                |
  | GND                  | GND               |

 
  Note
  ----
  This sketch uses the hardware serial port (USB) for Blender communication.
  Do **not** open the Serial Monitor or any other program that uses the
  same COM port while Blender is connected, the port can only be used
  by one application at a time.
*/

#include "blendixserial.h"  
#include <Servo.h>          

blendixserial blendix;       // Instance to parse Blender data

Servo myServo;               // Servo object

int lastServoPos = -1;       // Stores last servo angle to avoid repeated writes

void setup()
{
  // Hardware serial (USB) receives data from Blender
  Serial.begin(9600);

  // Attach the servo to pin 9
  myServo.attach(9);
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

    // Only move the servo if the angle actually changed
    if (servoPos != lastServoPos)
    {
      myServo.write(servoPos);   // Command the servo to the new angle
      lastServoPos = servoPos;   // Update last stored position
    }
  }
}