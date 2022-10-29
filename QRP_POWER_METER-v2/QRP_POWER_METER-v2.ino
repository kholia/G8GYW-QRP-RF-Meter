// https://g8gyw.github.io/
//
// Copyright (c) 2021 Mike G8GYW
// Copyright (c) 2022 Dhiru Kholia (VU3CER) - Port to RP2040-Zero board
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// This program is designed to run on an Atmega328P with internal 8MHz clock
// and internal bandgap voltage reference.  It takes the outputs from a
// Stockton bridge using a 10:1 transformer turns ratio and 1N5711 Scottky
// diodes, applies calibration factors then calculates and displays the average
// forward power, VSWR and battery voltage.
//
// RP2040-Zero version notes:
// - Vfwd comes to ADC1
// - Vrev comes to ADC0
// https://github.com/earlephilhower/arduino-pico/blob/master/variants/generic/common.h

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include "Filter.h"

#define VERSION   "v1.07"
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// DEFINE VARIABLES
double Vfwd;   // Forward voltage
double Vrev;   // Reverse voltage
double Pfwd;   // Forward power
double Prev;   // Reverse power
double SWR;    // VSWR
double Gamma;  // Reflection coefficient
double Vbat;   // Battery voltage

// CALIBRATION FACTORS
// The tolerance on the Atmega328P's internal voltage reference is 1.0 to 1.2 Volts.
// The actual voltage should be measured on pin 21 and entered below.

// The response of the 1N5711 diodes follows a curve represented by the equation:
// Pfwd = aVfwd^2 + bVfwd (and the same for Prev and Vrev)
// where a and b are constants determined by plotting Power In vs Vfwd and performing a curve fit.
// An excellent tool for this can be found at https://veusz.github.io/
// The values of a and b below can be adjusted if necessary to improve accuracy (try adjusting b first).
// Dhiru - Use https://keisan.casio.com/exec/system/14059932254941 for quadratic regression analysis.

// const double IntRef = 1.1;
// const double IntRef = 3.3; // for RP2040
// const double a = 1.0;
// const double b = 0.75;

// https://keisan.casio.com/exec/system/14059932254941
const double a = 1.1378;
const double b = 1.2538;
const double c = -0.08555;

// https://github.com/earlephilhower/arduino-pico/blob/master/docs/analog.rst
// Dhiru: 1023 is returned by analogRead() when ADC port is connected to 3.3v
const double conversion_factor = 3.3f / 1024; // Default resolution is 10bit

// FUNCTIONS TO CALCULATE FORWARD AND REVERSE POWER
double CalculatePfwd()
{
  /* double Vadc0 = analogRead(A0); // Read ADC0 (pin 23)
    Vadc0 = constrain(Vadc0, 1, 1023); // Prevent divide-by-zero when calculating Gamma
    Vfwd = 2.8 * ((Vadc0 + 0.5) * IntRef / 1024); // Scaled up by R3 & R4
    Pfwd = a * sq(Vfwd) + b * Vfwd; */

  double Vadc = analogRead(A1); // Read ADC1
  Vadc = constrain(Vadc, 1, (1 << 10) - 1); // Prevent divide-by-zero when calculating Gamma
  Vfwd = 2.8 * ((Vadc + 0.5) * conversion_factor); // Scaled up by R3 & R4
  // Serial.println(Vadc);
  // Serial.println(Vfwd);
  Pfwd = a * sq(Vfwd) + b * Vfwd + c;
  if (Pfwd < 0) {
    Prev = 0;
  }
  Serial.println(Pfwd);

  return Pfwd;
}

double CalculateSWR()
{
  /* double Vadc1 = analogRead(A1); // Read ADC1 (pin 24)
    Vrev = 2.8 * ((Vadc1 + 0.5) * IntRef / 1024); // Scaled up by R7 & R8 */

  double Vadc = analogRead(A0); // Read ADC0
  Vrev = 2.8 * ((Vadc + 0.5) * conversion_factor); // Scaled up by R7 & R8 */
  Prev = a * sq(Vrev) + b * Vrev + c;
  if (Prev < 0.01) { // exclude residual values
    Prev = 0;
  }
  Serial.println(Prev);
  Gamma = sqrt(Prev / Pfwd); // Calculate reflection coefficient
  SWR = (1 + Gamma) / (1 - Gamma);
  SWR = constrain(SWR, 1, 99.9);
  Serial.println(SWR);
  return SWR;
}

// ADC FILTER
// A recursive filter removes jitter from the readings using the MegunoLINK filter library:
// https://www.megunolink.com/documentation/arduino-libraries/exponential-filter/

// Create new exponential filters with a weight of 90 and initial value of 0
// Adjust these values as required for a stable display

ExponentialFilter<double> FilteredPfwd(50, 0);
ExponentialFilter<double> FilteredVSWR(50, 0);

void setup()
{
  // I2C display
  Wire.setSDA(0);
  Wire.setSCL(1);
  Wire.begin();

  /* analogRead (A2); // Read ADC2
    delay (500); // Allow ADC to settle
    double Vpot = analogRead (A2); // Read ADC again
    // Vbat = 4.9 * ((Vpot + 0.5) * conversion_factor); // Calculate battery voltage scaled by R9 & R10 */

  Serial.begin(115200);
  Serial.println("Hi!");

  // Display startup screen
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Initialize with the I2C address 0x3C.
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print("   POWER/SWR METER   ");
  display.setCursor(0, 16);
  display.print("    12 Watts maximum    ");
  display.setCursor(0, 32);
  display.print("Battery volts = ");
  display.print(Vbat);
  display.setCursor(0, 48);
  display.print("--------");
  display.print(VERSION);
  display.print("--------");
  display.display();
  // delay(5000); // delay 5 seconds
}

void loop()
{
  double Power = CalculatePfwd(); // Get new value of forward power
  FilteredPfwd.Filter(Power); // Apply filter to new value
  double SmoothPower = FilteredPfwd.Current(); // Return current value of filter output

  double VSWR = CalculateSWR(); // Get new value of VSWR
  FilteredVSWR.Filter(VSWR); // Apply filter to new value
  double SmoothVSWR = FilteredVSWR.Current(); // Return current value of filter output

  // Display Power and VSWR
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor (12, 12);
  display.print("PWR: ");
  display.setCursor (60, 12);
  // display.print(SmoothPower, 1); // Display forward power to one decimal place
  display.print(Power, 1); // Display forward power to one decimal place
  display.print("W");
  display.setCursor (12, 36);
  display.print("SWR: ");
  display.setCursor (60, 36);
  if (Pfwd < 0.01) {
    display.print("**.*"); // Blank invalid readings
  }
  else {
    // display.print(SmoothVSWR, 1); // Display VSWR to one decimal place
    display.print(VSWR, 1); // Display VSWR to one decimal place
  }
  display.display();
}
