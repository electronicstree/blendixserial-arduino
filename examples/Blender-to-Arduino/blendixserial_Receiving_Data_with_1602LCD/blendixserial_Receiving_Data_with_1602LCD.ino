/*
  Blender-to-Arduino | Object Position to 16x2 LCD (Parallel)

  Author   : Usman
  Date     : 25 March 2026
  Website  : www.electronicstree.com
  Email    : help@electronicstree.com

  Description
  -----------
  This example uses the BlendixSerial library to receive position data
  for Object 0 from Blender via USB. The X, Y, and Z position values
  are displayed on a standard 16×2 parallel LCD.

  LCD format:
      Row 1: "X: -45  Y: 180"
      Row 2: "Z:  90"

  Values are updated whenever Blender sends a new frame.


  Wiring
  ------

  | LCD Pin | Arduino Pin | Description          |
  |---------|-------------|----------------------|
  | 1 (VSS) | GND         | Ground               |
  | 2 (VDD) | 5V          | Power                |
  | 3 (V0)  | Potentiometer | Contrast adjustment|
  | 4 (RS)  | 12          | Register select      |
  | 5 (RW)  | GND         | Read/Write (ground)  |
  | 6 (EN)  | 11          | Enable               | 
  | 11 (D4) | 5           | Data bit 4           |
  | 12 (D5) | 4           | Data bit 5           |
  | 13 (D6) | 3           | Data bit 6           |
  | 14 (D7) | 2           | Data bit 7           |
  | 15 (A)  | 5V via 220Ω | Backlight (optional) |
  | 16 (K)  | GND         | Backlight ground     |


  Note
  ----
  - Uses hardware serial (USB) for Blender communication.
  - Do **not** open the Serial Monitor while Blender is connected — the
    COM port can only be used by one application at a time.
  - Install required libraries:
        * blendixserial 
        * LiquidCrystal (built‑in)
*/

#include <LiquidCrystal.h>    
#include "blendixserial.h"   

// LCD pin definitions (connect these to the corresponding pins on the LCD module)
#define LCD_RS   12          
#define LCD_EN   11           
#define LCD_D4   5            
#define LCD_D5   4          
#define LCD_D6   3            
#define LCD_D7   2           

// Create LCD object with the defined pins
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

blendixserial blendix;        // Blender data parser instance

// Store previous location or position values to avoid unnecessary LCD updates
float lastX = 0, lastY = 0, lastZ = 0;

void setup()
{
  // Initialize hardware serial (USB) for communication with Blender
  Serial.begin(9600);         // Must match the baud rate used in Blender's serial output

  // Initialize LCD: 16 columns, 2 rows
  lcd.begin(16, 2);
  lcd.clear();

  // Display startup message
  lcd.setCursor(0, 0);
  lcd.print("Waiting for");
  lcd.setCursor(0, 1);
  lcd.print("Blender data...");
  delay(2000);                // Show message for 2 seconds
  lcd.clear();
}

void loop()
{
  // Process all incoming bytes from Blender
  while (Serial.available())
  {
    // Feed each byte to the Blendix parser
    // When a complete frame arrives, bodParse() returns true
    if (blendix.bodParse(Serial.read()))
    {
      // Immediately read rotation data for Object 0 and update the LCD
      updateDisplay();
    }
  }
}

// Function to read the latest location values and update the LCD if they changed
void updateDisplay()
{
  float x, y, z;               // Variables to store location angles (X, Y, Z)

  // Uncomment ONE of the following lines to select the property to display:
  blendix.getLocation(0, x, y, z);   // Location (position)
  // blendix.getRotation(0, x, y, z); // Rotation (degree angles)
  // blendix.getScale(0, x, y, z);    // Scale

  // Only update the LCD if any value has changed (reduces flicker and unnecessary writes)
  if (x != lastX || y != lastY || z != lastZ)
  {
    lastX = x;
    lastY = y;
    lastZ = z;

    // Row 1: Display X and Y
    lcd.setCursor(0, 0);
    lcd.print("X:");
    lcd.print(x, 2);           // Show X with 2 decimal places
    lcd.print("   ");          // Clear any leftover characters

    lcd.setCursor(8, 0);
    lcd.print("Y:");
    lcd.print(y, 2);
    lcd.print("   ");

    // Row 2: Display Z
    lcd.setCursor(0, 1);
    lcd.print("Z:");
    lcd.print(z, 2);
    lcd.print("      ");       // Clear any leftover characters
  }
}