/*
  Arduino-to-Blender | Bouncing Ball with Squash & Stretch

  Author   : Usman
  Date     : 25 March 2026
  Website  : www.electronicstree.com
  Email    : help@electronicstree.com

  Description
  -----------
  This sketch sends a bouncing ball animation to Blender, including realistic
  physics, squash/stretch deformation, and a slight tumble during the fall.
  The ball bounces on the Z‑axis (vertical) with gravity, bounces, and gradually
  loses energy. When it hits the ground, it squashes; when falling, it stretches
  and tumbles. This demonstrates advanced animation techniques using the
  BlendixSerial library.

  
  How It Works
  ------------
  - Simple Euler integration of vertical motion with gravity.
  - When the ball reaches the floor, it reverses velocity (bounce) and
    squashes: scaleZ decreases while scaleX and scaleY increase.
  - While falling (above ground), the ball stretches (scaleZ > 1, scaleX/Y < 1)
    and rotates slightly to give a sense of speed.
  - When the bounce becomes very small (velocity < 0.1), the ball resets to a
    height of 5.0 to repeat the bounce.

 
  Wiring & Setup
  --------------
  Connect Arduino to PC via USB. In Blender, set the blendixserial add‑on to receive
  data on the same COM port at 115200 baud.


  Customization
  -------------
  - Adjust gravity, bounce, and dt to change the bounce behavior.
  - Modify squashFactor and stretchFactor multipliers for stronger/weaker
    deformation.
  - The tumble rotation can be sped up by changing the multiplier in rotX.
*/

#include "blendixserial.h"

blendixserial blendix;
uint8_t buf[BLENDIXSERIAL_MAX_PAYLOAD + 7];

// ========== Physics Variables ==========
float posZ = 5.0;          // Current height (Z axis in Blender)
float velocityZ = 0.0;     // Current vertical speed (m/s)
const float gravity = -9.8; // Gravity pulling downward (negative)
const float bounce = 0.75;   // Coefficient of restitution (energy retained)
const float floorY = 0.0;    // Ground level (where Z = 0)

// ========== Timing ==========
unsigned long lastMillis = 0;
const float dt = 0.016;    // Fixed timestep: 16 ms ≈ 60 FPS

void setup() {
  Serial.begin(115200);    // Must match Blender's baud rate
  lastMillis = millis();
}

void loop() {
  // Maintain a fixed update rate of about 60 Hz
  if (millis() - lastMillis >= 16) {
    lastMillis = millis();

    // --------------------------------------------------------------------
    // 1. PHYSICS INTEGRATION (Euler)
    // --------------------------------------------------------------------
    velocityZ += gravity * dt;   // Apply gravity to velocity
    posZ += velocityZ * dt;      // Update position

    // --------------------------------------------------------------------
    // 2. DEFORMATION & BOUNCE LOGIC
    // --------------------------------------------------------------------
    float scaleX = 1.0, scaleY = 1.0, scaleZ = 1.0;
    float rotX = 0.0;            // Tumble rotation around X axis

    if (posZ <= floorY) {
      // ---- COLLISION (bounce) ----
      posZ = floorY;                     // Snap to ground
      velocityZ *= -bounce;              // Reverse direction with energy loss

      // "Squash" effect: flatten along Z, expand X and Y
      // The faster the impact, the more pronounced the squash
      float squashFactor = abs(velocityZ) * 0.05;
      scaleZ = 1.0 - squashFactor;       // Squash vertical dimension
      scaleX = 1.0 + squashFactor;       // Widen horizontally
      scaleY = 1.0 + squashFactor;
    } else {
      // ---- FALLING (stretch & tumble) ----
      // "Stretch" effect: elongate along Z, narrow X and Y
      float stretchFactor = abs(velocityZ) * 0.02;
      scaleZ = 1.0 + stretchFactor;      // Stretch vertically
      scaleX = 1.0 - (stretchFactor * 0.5);
      scaleY = 1.0 - (stretchFactor * 0.5);

      // Add a slow tumble while falling – speed proportional to velocity
      // Use millis() to get a continuous rotation, but to make it dependent
      // on speed, we could also use a factor of velocity. Here we use a constant
      // rate for simplicity.
      rotX = millis() * 0.1;             // Continuous rotation around X
    }

    // --------------------------------------------------------------------
    // 3. RESET WHEN BOUNCE STOPS (optional)
    // --------------------------------------------------------------------
    if (abs(velocityZ) < 0.1 && posZ <= floorY) {
      // The ball has practically stopped – start a new bounce from a height
      posZ = 5.0;
      velocityZ = 0.0;
    }

    // --------------------------------------------------------------------
    // 4. SEND TRANSFORMS TO BLENDER
    // --------------------------------------------------------------------
    blendix.setLocation(0, 0, 0, posZ);          // Move only on Z axis
    blendix.setRotation(0, rotX, 0, 0);         // Tumble on X
    blendix.setScale(0, scaleX, scaleY, scaleZ); // Apply squash/stretch

    // Build and send the packet
    uint16_t len = blendix.bodBuild(buf);
    if (len > 0) {
      Serial.write(buf, len);
    }
  }
}