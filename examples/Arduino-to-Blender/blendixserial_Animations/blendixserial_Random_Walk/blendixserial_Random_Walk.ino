/*
  Arduino-to-Blender | Random Walk 

  Author   : Usman
  Date     : 25 March 2026
  Website  : www.electronicstree.com
  Email    : help@electronicstree.com

  Description
  -----------
  This sketch creates an object that moves in a random, meandering path
  inside a soft‑bounded area. Every frame, the direction changes by a small
  random angle, and the object takes a step forward. When it reaches a
  boundary (±5 units on X or Y), it "bounces" by reversing its direction,
  keeping it inside the area. A tiny random scale jitter adds a natural,
  organic tremor.

  The result is a wandering creature or a floating particle that feels alive
  and never repeats its motion.


  How It Works
  ------------
  - The loop runs at 50 Hz (20 ms per frame) for smooth motion.
  - At each frame, the current angle is perturbed by a random amount
    between -20° and +20°.
  - The object then moves forward by a fixed step (0.05 units) in the
    new direction.
  - If the X or Y position exceeds ±5.0, the angle is flipped by 180°,
    causing the object to bounce away from the wall.
  - A small random scale jitter (±3% of the original size) adds a subtle
    pulsing effect.
  - Rotation is set to the current movement angle, so the object always
    faces the direction it’s moving.


  Wiring & Setup
  --------------
  Connect Arduino to PC via USB. In Blender, set the bledixserial add‑on to receive
  data on the same COM port at 115200 baud.


  Customization
  -------------
  - Step size: change the 0.05 constant to adjust movement speed.
  - Random angle range: modify the random(-20,21) to control how drastically
    the path changes.
  - Boundaries: adjust the 5.0 constants to change the roaming area.
  - Jitter amplitude: change the random(-3,3) range to increase/decrease
    scale variation.
  - Jitter frequency: because it changes every frame, it’s very fast; you can
    slow it by updating jitter only every few frames (not shown).
*/

#include "blendixserial.h"

blendixserial blendix;
uint8_t buf[BLENDIXSERIAL_MAX_PAYLOAD + 7];

// ========== Animation Variables ==========
float x = 0, y = 0;            // Current position (X, Y)
float angle = 0;               // Current movement direction (radians)
unsigned long lastMillis = 0;  // For frame timing

void setup() {
  Serial.begin(115200);        // Must match Blender's baud rate
  randomSeed(analogRead(0));   // Seed the random generator for true variety
  lastMillis = millis();
}

void loop() {
  // Fixed update rate of 50 Hz (20 ms per frame)
  if (millis() - lastMillis >= 20) {
    lastMillis = millis();

    // --------------------------------------------------------------------
    // 1. CHANGE DIRECTION SLIGHTLY
    //    Add a random angle between -20° and +20° to the current heading.
    //    Convert degrees to radians for consistency.
    // --------------------------------------------------------------------
    angle += random(-20, 21) * (PI / 180.0);

    // --------------------------------------------------------------------
    // 2. STEP FORWARD
    //    Move a fixed distance (0.05 units) in the new direction.
    // --------------------------------------------------------------------
    x += cos(angle) * 0.05;
    y += sin(angle) * 0.05;

    // --------------------------------------------------------------------
    // 3. SOFT BORDERS (Bounce off invisible walls)
    //    If the object goes beyond the ±5.0 boundary on X or Y, we flip
    //    the movement direction by 180° (π radians). This creates a smooth
    //    rebound instead of a hard stop.
    // --------------------------------------------------------------------
    if (abs(x) > 5.0) angle += PI;   // Bounce on X boundary
    if (abs(y) > 5.0) angle += PI;   // Bounce on Y boundary

    // --------------------------------------------------------------------
    // 4. RANDOM JITTER SCALE
    //    A tiny random scale variation (±3% of 1.0) adds an organic tremor,
    //    making the object look like it's alive or trembling slightly.
    // --------------------------------------------------------------------
    float jitter = 1.0 + (random(-3, 3) / 100.0);   // Range 0.97 – 1.03

    // --------------------------------------------------------------------
    // 5. SEND TRANSFORMS TO BLENDER
    // --------------------------------------------------------------------
    blendix.setLocation(0, x, y, 0);               // Z stays at 0
    blendix.setRotation(0, 0, 0, angle * 180 / PI); // Convert angle to degrees
    blendix.setScale(0, jitter, jitter, jitter);    // Uniform scale with jitter

    uint16_t len = blendix.bodBuild(buf);
    if (len > 0) {
      Serial.write(buf, len);                      // Send packet to Blender
    }
  }
}