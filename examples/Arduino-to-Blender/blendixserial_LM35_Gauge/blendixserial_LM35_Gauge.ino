/*
  Arduino-to-Blender | LM35 Temperature Gauge

  Author   : Usman
  Date     : 25 March 2026
  Website  : www.electronicstree.com
  Email    : help@electronicstree.com

  Description
  -----------
  This sketch reads an LM35 temperature sensor (analog pin A0) and sends the
  measured temperature (in °C) to Blender as a rotation around the Y‑axis.
  The value is sent only when it changes, reducing serial traffic.


  Wiring
  ------
  LM35:
      VCC → 5V
      GND → GND
      OUT → A0

  Arduino:
      USB → PC (for Blender communication)


  Setup in Blender
  ----------------
  1. Install the blendixserial add‑on.
  2. Set the COM port and baud rate (9600).
  3. Choose "Receive Data".
  4. Assign an object (e.g., a gauge needle) to Object ID 0.
  5. Enable "Rotation" for that object.
  6. Start the connection – the object's Y‑rotation will reflect the temperature.
*/

#include "blendixserial.h"

blendixserial blendix;
uint8_t buf[BLENDIXSERIAL_MAX_PACKET_SIZE];  // Packet buffer

float prev_temperature = -1.0;               // Last sent temperature

void setup() {
  Serial.begin(9600);                        // Must match Blender baud rate
}

void loop() {
  // Read LM35 (analog pin A0)
  int raw = analogRead(A0);
  float voltage = raw * (5.0 / 1023.0);
  float temperature = voltage * 100.0;       // LM35 outputs 10mV per °C

  // Round to one decimal place to avoid sending too many tiny changes
  float rounded = round(temperature * 10.0) / 10.0;

  // Send only when the value actually changes
  if (rounded != prev_temperature) {
    // Set the Y‑rotation of object 0 to the temperature value (in degrees)
    blendix.setRotation(0, 0.0, rounded, 0.0);

    // Build the packet
    uint16_t len = blendix.bodBuild(buf);
    if (len > 0) {
      // Send the raw packet over the hardware serial
      Serial.write(buf, len);
    }

    // Update the stored value
    prev_temperature = rounded;
  }

  // Wait a bit before next reading
  delay(100);
}