/*
 * Basic BLDC Motor Control Example
 * 
 * This example shows how to control a BLDC motor with the LayeredQueue library.
 * Works on ESP32, SAMD21, SAMD51, and STM32F4 boards.
 * 
 * Hardware connections:
 * - ESP32: AH=GPIO25, AL=GPIO26, BH=GPIO27, BL=GPIO14, CH=GPIO12, CL=GPIO13
 * - SAMD21/51: AH=D2, AL=D3, BH=D4, BL=D5, CH=D6, CL=D7
 * - STM32: AH=PA8, AL=PA7, BH=PA9, BL=PB0, CH=PA10, CL=PB1
 */

#include <LayeredQueue.h>

// Motor configuration
struct lq_bldc_config motor_config;
struct lq_bldc_ctx motor;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  
  Serial.println("LayeredQueue - Basic BLDC Motor Example");
  
  // Configure motor
  motor_config.pole_pairs = 7;        // Number of pole pairs in your motor
  motor_config.pwm_frequency = 20000; // 20 kHz PWM
  motor_config.deadtime_ns = 500;     // 500ns deadtime
  motor_config.max_duty = 950;        // 95% max duty cycle
  
  // Initialize motor
  if (lq_bldc_init(&motor, &motor_config) != 0) {
    Serial.println("ERROR: Failed to initialize motor!");
    while (1) { delay(100); }
  }
  
  Serial.println("Motor initialized successfully!");
  Serial.println("Ramping up motor speed...");
}

void loop() {
  static uint32_t last_update = 0;
  static uint16_t speed_rpm = 0;
  static bool ramping_up = true;
  
  // Update speed every 100ms
  if (millis() - last_update >= 100) {
    last_update = millis();
    
    // Ramp speed up and down
    if (ramping_up) {
      speed_rpm += 50;
      if (speed_rpm >= 2000) {
        ramping_up = false;
        Serial.println("Max speed reached, ramping down...");
      }
    } else {
      if (speed_rpm > 50) {
        speed_rpm -= 50;
      } else {
        ramping_up = true;
        Serial.println("Min speed reached, ramping up...");
      }
    }
    
    // Set motor speed and 50% throttle
    lq_bldc_set_speed(&motor, speed_rpm);
    lq_bldc_set_throttle(&motor, 500); // 0-1000 scale (50%)
    
    Serial.print("Speed: ");
    Serial.print(speed_rpm);
    Serial.println(" RPM");
  }
  
  // Update motor commutation (call frequently!)
  lq_bldc_update(&motor, micros());
}
