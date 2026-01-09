/*
 * Simple BLDC Motor Control for ESP32
 * 
 * Compatible with:
 * - ESP32 DevKit
 * - ESP32-S3 DevKit
 * - Any ESP32 board with MCPWM peripheral
 * 
 * Uses MCPWM peripheral for complementary PWM
 */

#include <LayeredQueue_BLDC.h>

BLDC_Motor motor(0);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("BLDC Motor Example - ESP32");
  
  // Configure pins for ESP32
  // MCPWM Unit 0
  // High-side: GPIO16, GPIO18, GPIO19
  // Low-side: GPIO17, GPIO5, GPIO4
  
  // For ESP32, gpio_port is not used (ESP32 has single port)
  // gpio_pin is the actual GPIO number
  motor.setHighSidePin(0, 0, 16, 0);   // Phase U  - GPIO16
  motor.setHighSidePin(1, 0, 18, 0);   // Phase V  - GPIO18
  motor.setHighSidePin(2, 0, 19, 0);   // Phase W  - GPIO19
  
  motor.setLowSidePin(0, 0, 17, 0);    // Phase U' - GPIO17
  motor.setLowSidePin(1, 0, 5, 0);     // Phase V' - GPIO5
  motor.setLowSidePin(2, 0, 4, 0);     // Phase W' - GPIO4
  
  // Initialize motor
  if (!motor.begin(3, 7, 25000, 1000)) {
    Serial.println("ERROR: Motor initialization failed!");
    while (1);
  }
  
  Serial.println("Motor initialized");
  Serial.println("Using MCPWM Unit 0");
  
  motor.setMode(LQ_BLDC_MODE_SINE);
  motor.enable(true);
  
  Serial.println("Motor enabled");
}

void loop() {
  static uint8_t throttle = 0;
  static bool increasing = true;
  
  motor.update();
  
  // Ramp throttle
  static uint32_t last_ramp = 0;
  if (millis() - last_ramp >= 50) {
    if (increasing) {
      throttle++;
      if (throttle >= 100) increasing = false;
    } else {
      throttle--;
      if (throttle == 0) increasing = true;
    }
    
    motor.setPower(throttle);
    last_ramp = millis();
  }
  
  // Print status
  static uint32_t last_print = 0;
  if (millis() - last_print >= 1000) {
    Serial.printf("Throttle: %d%%\n", throttle);
    last_print = millis();
  }
  
  delay(1);
}
