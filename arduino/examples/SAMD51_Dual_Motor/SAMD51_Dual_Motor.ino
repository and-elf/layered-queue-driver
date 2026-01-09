/*
 * Dual BLDC Motor Control for SAMD51 boards
 * 
 * Compatible with:
 * - Adafruit Metro M4
 * - Adafruit Grand Central M4
 * - SAMD51 Thing Plus
 * 
 * Uses TCC0 and TCC1 for independent motor control
 */

#include <LayeredQueue_BLDC.h>

// Create two motor instances
BLDC_Motor motor0(0);  // Motor 0 on TCC0
BLDC_Motor motor1(1);  // Motor 1 on TCC1

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);
  
  Serial.println("Dual BLDC Motor Example - SAMD51");
  
  // Configure Motor 0 (TCC0)
  motor0.setHighSidePin(0, 1, 12, 0x04);  // PB12 - TCC0/WO[0]
  motor0.setHighSidePin(1, 1, 13, 0x04);  // PB13 - TCC0/WO[1]
  motor0.setHighSidePin(2, 0, 20, 0x04);  // PA20 - TCC0/WO[2]
  
  motor0.setLowSidePin(0, 0, 14, 0x04);   // PA14 - TCC0/WO[4]
  motor0.setLowSidePin(1, 0, 15, 0x04);   // PA15 - TCC0/WO[5]
  motor0.setLowSidePin(2, 0, 16, 0x04);   // PA16 - TCC0/WO[6]
  
  // Configure Motor 1 (TCC1)
  motor1.setHighSidePin(0, 0, 16, 0x05);  // PA16 - TCC1/WO[0], function F
  motor1.setHighSidePin(1, 0, 17, 0x05);  // PA17 - TCC1/WO[1], function F
  motor1.setHighSidePin(2, 0, 18, 0x05);  // PA18 - TCC1/WO[2], function F
  
  motor1.setLowSidePin(0, 1, 10, 0x05);   // PB10 - TCC1/WO[4], function F
  motor1.setLowSidePin(1, 1, 11, 0x05);   // PB11 - TCC1/WO[5], function F
  motor1.setLowSidePin(2, 0, 19, 0x05);   // PA19 - TCC1/WO[6], function F
  
  // Initialize both motors
  if (!motor0.begin(3, 7, 25000, 1000)) {
    Serial.println("ERROR: Motor 0 initialization failed!");
    while (1);
  }
  Serial.println("Motor 0 initialized");
  
  if (!motor1.begin(3, 7, 25000, 1000)) {
    Serial.println("ERROR: Motor 1 initialization failed!");
    while (1);
  }
  Serial.println("Motor 1 initialized");
  
  // Set modes
  motor0.setMode(LQ_BLDC_MODE_SINE);
  motor1.setMode(LQ_BLDC_MODE_SINE);
  
  // Enable both motors
  motor0.enable(true);
  motor1.enable(true);
  
  Serial.println("Both motors enabled");
}

void loop() {
  static uint8_t throttle0 = 0;
  static uint8_t throttle1 = 0;
  static bool increasing = true;
  
  // Update both motors
  motor0.update();
  motor1.update();
  
  // Independent control for each motor
  static uint32_t last_update = 0;
  if (millis() - last_update >= 20) {
    // Motor 0: Ramp up and down
    if (increasing) {
      throttle0++;
      if (throttle0 >= 80) increasing = false;
    } else {
      throttle0--;
      if (throttle0 == 0) increasing = true;
    }
    
    // Motor 1: Follow at half speed
    throttle1 = throttle0 / 2;
    
    motor0.setPower(throttle0);
    motor1.setPower(throttle1);
    
    last_update = millis();
  }
  
  // Print status
  static uint32_t last_print = 0;
  if (millis() - last_print >= 500) {
    Serial.print("Motor 0: ");
    Serial.print(throttle0);
    Serial.print("% | Motor 1: ");
    Serial.print(throttle1);
    Serial.println("%");
    last_print = millis();
  }
  
  delay(1);
}
