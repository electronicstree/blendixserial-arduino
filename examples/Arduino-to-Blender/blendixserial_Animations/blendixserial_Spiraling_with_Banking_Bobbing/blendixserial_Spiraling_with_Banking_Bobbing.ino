/*
  Arduino-to-Blender | Spiraling Animation with Banking & Bobbing

  Author   : Usman
  Date     : 25 March 2026
  Website  : www.electronicstree.com
  Email    : help@electronicstree.com

  Description
  -----------
  This sketch sends a dynamic spiral animation to Blender. The object moves in a
  circular path whose radius slowly expands and contracts. It also bobs up and down,
  banks into the turn (tilts), and continuously rotates around its Z axis.

  The result is a smooth, complex motion reminiscent of a flying creature or
  a dynamic camera path.


  How It Works
  ------------
  - The loop runs at 50 Hz (20 ms per frame) for smooth motion.
  - An internal timer is incremented by 0.02 s each frame.
  - Spiral radius: sin(timer * 0.5) causes the radius to oscillate between 2.0 and 6.0,
    expanding and contracting every ~12.6 seconds.
  - Circular path: X and Y are driven by cos/sin of timer * 2.5, creating a fast loop.
  - Vertical bobbing: Z uses cos(timer * 1.2) to smoothly rise and fall.
  - Banking tilt: tiltX (rotation around X) follows the same fast sine as the circular
    motion, making the object lean into the turn.
  - Continuous rotation: rotZ increases linearly with the timer, spinning the object
    around its vertical axis.
  - Scale is kept constant at 1.0 (no scaling).


  Wiring & Setup
  --------------
  Connect Arduino to PC via USB. In Blender, set the blendixsrial add‑on to receive
  data on the same COM port at 115200 baud.

 
  Customization
  -------------
  - Radius oscillation: adjust the multiplier (0.5) to change the expansion speed.
  - Radius range: modify the constants (4.0 + 2.0 * sin(...)) to control min/max.
  - Circular speed: change the frequency (2.5) to make the loop faster/slower.
  - Bobbing: adjust the amplitude (1.5) and frequency (1.2) in Z.
  - Banking tilt: change the amplitude (15.0) and frequency (2.5) to match the turn.
  - Rotation speed: rotZ multiplier (2.5) controls how fast it spins.
*/

#include "blendixserial.h"

blendixserial blendix;
uint8_t buf[BLENDIXSERIAL_MAX_PAYLOAD + 7];

// ========== Animation Variables ==========
float timer = 0;               // Internal time (seconds)
unsigned long lastMillis = 0;  // For frame timing

void setup() {
  Serial.begin(115200);        // Must match Blender's baud rate
  lastMillis = millis();
}

void loop() {
  // Fixed update rate of 50 Hz (20 ms per frame)
  if (millis() - lastMillis >= 20) {
    lastMillis = millis();
    timer += 0.02;             // Advance time by the frame interval

    // --------------------------------------------------------------------
    // 1. SPIRAL RADIUS (slowly expanding and contracting)
    //    sin(timer * 0.5) gives a period of about 12.6 seconds.
    //    Radius oscillates between 2.0 and 6.0.
    // --------------------------------------------------------------------
    float radius = 4.0 + 2.0 * sin(timer * 0.5);

    // --------------------------------------------------------------------
    // 2. CIRCULAR PATH (X and Y)
    //    timer * 2.5 completes one full circle every ~2.5 seconds.
    //    The object moves along this circle, while the radius changes,
    //    creating a spiral effect.
    // --------------------------------------------------------------------
    float x = radius * cos(timer * 2.5);
    float y = radius * sin(timer * 2.5);

    // --------------------------------------------------------------------
    // 3. VERTICAL BOBBING (Z)
    //    A slower sine wave (timer * 1.2) gives a gentle up‑down motion.
    //    Amplitude 1.5 makes it bob by 1.5 units above and below the center.
    // --------------------------------------------------------------------
    float z = 1.5 * cos(timer * 1.2);

    // --------------------------------------------------------------------
    // 4. BANKING TILT (rotation around X axis)
    //    The object leans into the turn. The tilt amplitude is 15 degrees,
    //    and it follows the same fast frequency as the circular motion,
    //    making it tilt outward when turning.
    // --------------------------------------------------------------------
    float tiltX = 15.0 * sin(timer * 2.5);

    // --------------------------------------------------------------------
    // 5. CONTINUOUS ROTATION (Z axis)
    //    The object spins around its vertical axis at a constant rate.
    //    timer * 2.5 gives about 2.5 revolutions per second.
    // --------------------------------------------------------------------
    float rotZ = (timer * 2.5) * 180 / PI;   // Convert radians to degrees

    // --------------------------------------------------------------------
    // 6. SEND TRANSFORMS TO BLENDER
    // --------------------------------------------------------------------
    blendix.setLocation(0, x, y, z);
    blendix.setRotation(0, tiltX, 0, rotZ);   // X rotation = banking, Z rotation = spin
    blendix.setScale(0, 1.0, 1.0, 1.0);       // No scaling

    uint16_t len = blendix.bodBuild(buf);
    if (len > 0) {
      Serial.write(buf, len);                // Send packet to Blender
    }
  }
}