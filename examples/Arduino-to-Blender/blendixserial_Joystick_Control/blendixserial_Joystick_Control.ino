/*
  Arduino-to-Blender | 2‑Axis Joystick 

  Author   : Usman
  Date     : 26 March 2026
  Website  : www.electronicstree.com
  Email    : help@electronicstree.com

  Description
  -----------
  This example demonstrates how to use an analog 2-axis joystick to
  control the position of an object in Blender in real time using the
  blendixserial library.

  The Arduino reads the joystick's X and Y analog values (0–1023),
  converts them into a user-defined coordinate range, and sends them
  to Blender as the Location of Object 0.

  Key Features
  ------------
  • Real-time control of a Blender object using physical input  
  • Smooth motion using a configurable update rate (50 Hz)  
  • Deadzone filtering to eliminate small noise and jitter  
  • Efficient communication using delta updates (only sends changes)  

  How It Works
  ------------
  1. The joystick outputs analog voltages on two axes:
       - VRx → horizontal movement (X axis)
       - VRy → vertical movement (Y axis)

  2. Arduino reads these values using analogRead().

  3. The raw range (0–1023) is mapped to a Blender coordinate range
     (e.g., -5.0 to +5.0 units).

  4. A deadzone filter ignores very small changes to prevent unwanted
     jitter and reduce serial traffic.

  5. The processed values are sent to Blender using:
         blendix.setLocation(objectID, x, y, z);

  6. The blendixserial library builds a compact binary packet and sends
     it over Serial to Blender.

  7. Blender receives the data and updates the object's position.

  Notes
  -----
  • Z position is fixed at 0 in this example (2D control).  
  • You can adjust OUT_MIN / OUT_MAX to change movement range.  
  • You can invert axes if movement direction feels reversed.  
  • Update rate (READ_INTERVAL_MS) controls responsiveness vs bandwidth.  

  Wiring
  ------
  Joystick Module (common type):
      VCC  → 5V
      GND  → GND
      VRx  → A0  (X‑axis)
      VRy  → A1  (Y‑axis)

  Arduino:
      USB → PC (for Blender communication)


  Setup in Blender
  ----------------
  1. Install the blendixserial add‑on.
  2. Set the COM port and baud rate (115200 – change in sketch if needed).
  3. Choose "Receive Data".
  4. Assign an object to Object ID 0.
  5. Enable "Location" for that object.
  6. Start the connection – the object will follow the joystick movement.
*/


#include "blendixserial.h"

// Joystick pins
const int JOY_X_PIN = A0;
const int JOY_Y_PIN = A1;

// Mapping: raw 0‑1023 → Blender coordinate range
const float IN_MIN = 0.0;
const float IN_MAX = 1023.0;
const float OUT_MIN = -5.0;   // left / down
const float OUT_MAX = 5.0;    // right / up

// Deadzone: ignore changes smaller than this (in Blender units)
const float DEADZONE = 0.05;

blendixserial blendix;
uint8_t buf[BLENDIXSERIAL_MAX_PACKET_SIZE];

float lastX = 0, lastY = 0;
unsigned long lastRead = 0;
const unsigned long READ_INTERVAL_MS = 20;   // 50 Hz

void setup() {
  Serial.begin(115200);   // Must match Blender baud rate
}

void loop() {
  if (millis() - lastRead >= READ_INTERVAL_MS) {
    lastRead = millis();

    // Read raw values
    int rawX = analogRead(JOY_X_PIN);
    int rawY = analogRead(JOY_Y_PIN);

    // Map to Blender coordinates
    float x = mapFloat(rawX, IN_MIN, IN_MAX, OUT_MIN, OUT_MAX);
    float y = mapFloat(rawY, IN_MIN, IN_MAX, OUT_MIN, OUT_MAX);

    // Optional: invert axes if needed
    // x = -x;
    // y = -y;

    // Only send if change is larger than deadzone
    if (abs(x - lastX) < DEADZONE && abs(y - lastY) < DEADZONE) {
      return;   // no significant change
    }

    // Send location (Z fixed at 0)
    blendix.setLocation(0, x, y, 0.0);

    uint16_t len = blendix.bodBuild(buf);
    if (len > 0) {
      Serial.write(buf, len);
    }

    lastX = x;
    lastY = y;
  }
}

// Helper: map a float value from one range to another
float mapFloat(float value, float inMin, float inMax, float outMin, float outMax) {
  return (value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}