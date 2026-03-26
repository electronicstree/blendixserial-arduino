/*
  Arduino-to-Blender | Heartbeat Animation (Lub-Dub)

  Author   : Usman
  Date     : 25 March 2026
  Website  : www.electronicstree.com
  Email    : help@electronicstree.com

  Description
  -----------
  This sketch sends a realistic heartbeat animation to Blender. It simulates the
  characteristic "lub‑dub" sound by creating two distinct expansions:
      - First beat (lub): a quick, strong expansion.
      - Second beat (dub): a slightly weaker, shorter expansion.
  Between the two beats there is a tiny pause, followed by a longer rest period.

  The animation also includes:
      - Organic jitter (tiny random variations) to make the pulse look more alive.
      - A subtle forward jump on the Y axis that mirrors the expansion.
      - Constant rotation (zero) – only scale and location are animated.

 
  How It Works
  ------------
  - A fixed frame rate of about 60 Hz (16 ms per update) ensures smooth motion.
  - The heartbeat cycle length is set to 1200 ms (1.2 s) – adjustable.
  - The millis() timer gives the current phase within the cycle (0…1199 ms).
  - The phase is split into four intervals:
        0 – 150 ms   : first expansion (lub)
        150 – 200 ms : short pause
        200 – 400 ms : second expansion (dub)
        400 – 1200 ms: rest
  - Within each expansion interval, a sine wave shapes the rise and fall.
  - Random jitter is added to make the animation feel organic.
  - The object moves slightly forward (positive Y) during expansion.

  
  Wiring & Setup
  --------------
  Connect Arduino to PC via USB. In Blender, set the blendixserial add‑on to receive
  data on the same COM port at 115200 baud.


  Customization
  -------------
  - HEART_RATE_MS: change the cycle length (e.g., 800 for faster, 1500 for slower).
  - Expansion magnitudes: modify the multipliers (0.3 for lub, 0.15 for dub).
  - Jitter range: adjust the random divisor (currently 1000) for more/less noise.
  - Forward jump: change the factor (0.5) in posY calculation.
*/

#include "blendixserial.h"

blendixserial blendix;
uint8_t buf[BLENDIXSERIAL_MAX_PAYLOAD + 7];

// ========== Animation Parameters ==========
const int HEART_RATE_MS = 1200;   // Full cycle time (milliseconds)
                                  // 1200 ms = 1.2 seconds → ~50 bpm

// ========== Timing ==========
unsigned long lastMillis = 0;
const int UPDATE_INTERVAL_MS = 16;   // ~60 FPS (60 Hz)

void setup() {
  Serial.begin(115200);          // Must match Blender's baud rate
  lastMillis = millis();
}

void loop() {
  // Maintain a consistent update rate (non‑blocking)
  if (millis() - lastMillis >= UPDATE_INTERVAL_MS) {
    lastMillis = millis();

    // --------------------------------------------------------------------
    // 1. PHASE CALCULATION
    //    Determine where we are in the heartbeat cycle (0 to HEART_RATE_MS-1)
    // --------------------------------------------------------------------
    long phase = millis() % HEART_RATE_MS;
    float scale = 1.0;           // Base scale (normal size)

    // --------------------------------------------------------------------
    // 2. DOUBLE BEAT (LUB-DUB) LOGIC
    // --------------------------------------------------------------------
    if (phase < 150) {
      // ---- First beat (Lub) – quick, strong expansion ----
      //   Uses a sine wave that peaks at the middle of the interval.
      //   t = phase / 150 → 0 … 1
      float t = phase / 150.0;
      scale = 1.0 + 0.3 * sin(t * PI);   // +30% at peak
    }
    else if (phase < 200) {
      // ---- Short pause between beats ----
      scale = 1.0;
    }
    else if (phase < 400) {
      // ---- Second beat (Dub) – slightly smaller expansion ----
      //   subPhase = (phase - 200) / 200 → 0 … 1
      float subPhase = (phase - 200) / 200.0;
      scale = 1.0 + 0.15 * sin(subPhase * PI);   // +15% at peak
    }
    else {
      // ---- Rest period (until the next cycle) ----
      scale = 1.0;
    }

    // --------------------------------------------------------------------
    // 3. ORGANIC JITTER (adds subtle randomness to feel alive)
    //    Random value between -0.01 and +0.01, added to scale.
    // --------------------------------------------------------------------
    float jitter = (random(-10, 10) / 1000.0);
    float finalScale = scale + jitter;

    // --------------------------------------------------------------------
    // 4. SUBTLE FORWARD MOVEMENT (on Y axis)
    //    The object jumps forward slightly in sync with the expansion.
    //    posY = (scale - 1) * 0.5 → only moves during expansion.
    // --------------------------------------------------------------------
    float posY = (finalScale - 1.0) * 0.5;   // Forward leap proportional to scale change

    // --------------------------------------------------------------------
    // 5. SEND TRANSFORMS TO BLENDER
    // --------------------------------------------------------------------
    blendix.setLocation(0, 0, posY, 0);          // Move forward (Y)
    blendix.setRotation(0, 0, 0, 0);             // No rotation
    blendix.setScale(0, finalScale, finalScale, finalScale);   // Uniform scale

    // Build the packet and send
    uint16_t len = blendix.bodBuild(buf);
    if (len > 0) {
      Serial.write(buf, len);
    }
  }
}