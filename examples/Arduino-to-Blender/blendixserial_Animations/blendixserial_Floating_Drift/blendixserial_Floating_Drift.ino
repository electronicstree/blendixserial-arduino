/*
  Arduino-to-Blender | Floating Drift Animation

  Author   : Usman
  Date     : 25 March 2026
  Website  : www.electronicstree.com
  Email    : help@electronicstree.com

  Description
  -----------
  This sketch sends a smooth, organic floating motion to Blender. It is designed
  to be applied to any object that should appear to drift gently in space,
  like a balloon, a hovering character, or a magical object.

  The animation uses three layers of motion:
      1. Drifting location – two sine waves per axis create pseudo‑random paths.
      2. Gentle rotation – the object slowly tilts (roll, pitch) in sync with the drift.
      3. Breathing scale – a very slow uniform expansion/contraction, as if the object is alive.


  How It Works
  ------------
  - The loop runs at 50 Hz (20 ms per frame) for smooth motion.
  - An internal time variable is incremented by 0.02 s each frame.
  - Location X and Y are each built from two sine waves with different frequencies,
    making the path look non‑repetitive.
  - Location Z uses a single faster sine wave to give a slight “bobbing” motion.
  - Rotation (roll and pitch) uses slower sine/cosine waves to tilt the object
    into its movement.
  - Scale is a slow sine wave (0.4 Hz) that pulses between 0.95 and 1.05.


  Wiring & Setup
  --------------
  Connect Arduino to PC via USB. In Blender, set the blenidxserial add‑on to receive
  data on the same COM port at 115200 baud.

  --------------------------------------------------------------------
  Customization
  --------------------------------------------------------------------
  - Drift amplitudes: adjust the multipliers (0.8, 0.4, 0.6, etc.) to change
    the wander range.
  - Drift frequencies: change the multipliers inside sin()/cos() to alter speed.
  - Rotation: modify roll/pitch amplitudes (10.0, 5.0) and frequencies.
  - Breathing: adjust amplitude (0.05) and frequency (0.4) for slower/faster pulsing.
*/

#include "blendixserial.h"

blendixserial blendix;
uint8_t buf[BLENDIXSERIAL_MAX_PAYLOAD + 7];

// ========== Timing ==========
unsigned long lastMillis = 0;
const int UPDATE_INTERVAL_MS = 20;   // 50 Hz (20 ms)
float time = 0.0;                    // Internal clock (seconds)

void setup() {
  Serial.begin(115200);              // Must match Blender's baud rate
  lastMillis = millis();
}

void loop() {
  // Fixed update rate
  if (millis() - lastMillis >= UPDATE_INTERVAL_MS) {
    lastMillis = millis();
    time += UPDATE_INTERVAL_MS / 1000.0;   // Increment time by actual elapsed seconds

    // --------------------------------------------------------------------
    // 1. DRIFTING LOCATION (layered sine waves for pseudo‑randomness)
    // --------------------------------------------------------------------
    // X axis: two sines with different frequencies to avoid obvious repetition
    float driftX = 0.8 * sin(time * 1.0) + 0.4 * sin(time * 0.7);
    // Y axis: mix of cosine and sine with different speeds
    float driftY = 0.6 * cos(time * 0.8) + 0.3 * sin(time * 1.2);
    // Z axis: faster vertical "bobbing"
    float driftZ = 0.5 * sin(time * 1.5);

    // --------------------------------------------------------------------
    // 2. GENTLE SWAY (rotation)
    //    The object leans into its drift, making it feel more organic.
    // --------------------------------------------------------------------
    float roll  = 10.0 * sin(time * 0.5);   // Side‑to‑side tilt (degrees)
    float pitch = 5.0  * cos(time * 0.9);   // Forward/back tilt (degrees)
    // yaw (rotation around Z) is kept at 0 for simplicity.

    // --------------------------------------------------------------------
    // 3. BREATHING SCALE (slow, subtle pulse)
    // --------------------------------------------------------------------
    float breathe = 1.0 + 0.05 * sin(time * 0.4);   // Scale between 0.95 and 1.05

    // --------------------------------------------------------------------
    // 4. SEND ALL TRANSFORMS TO BLENDER
    // --------------------------------------------------------------------
    blendix.setLocation(0, driftX, driftY, driftZ);
    blendix.setRotation(0, roll, pitch, 0);   // roll = X rotation, pitch = Y rotation
    blendix.setScale(0, breathe, breathe, breathe);

    // Build packet and transmit
    uint16_t len = blendix.bodBuild(buf);
    if (len > 0) {
      Serial.write(buf, len);
    }
  }
}