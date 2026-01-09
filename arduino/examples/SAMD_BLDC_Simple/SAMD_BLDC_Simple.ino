/*
 * Simple BLDC Motor Control for Arduino SAMD boards
 * 
 * Compatible with:
 * - Arduino Zero
 * - Arduino MKR series
 * - Adafruit Feather M0/M4
 * - Adafruit Metro M4
 * 
 * Hardware:
 * - 3-phase BLDC motor
 * - MOSFET driver board (DRV8323, TMC6200, etc.)
 * - Power supply for motor
 */

#include <LayeredQueue_BLDC.h>

// Create motor instance
BLDC_Motor motor(0);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);  // Wait up to 3s for serial
  
  Serial.println("BLDC Motor Example - Arduino SAMD");
  
  // Configure pins for SAMD21 (Arduino Zero, MKR)
  // High-side: PA04, PA05, PA06
  // Low-side: PA10, PA11, PA12
  motor.setHighSidePin(0, 0, 4, 0x04);   // Phase U  - PA04, TCC0/WO[0], function E
  motor.setHighSidePin(1, 0, 5, 0x04);   // Phase V  - PA05, TCC0/WO[1], function E
  motor.setHighSidePin(2, 0, 6, 0x04);   // Phase W  - PA06, TCC0/WO[2], function E
  
  motor.setLowSidePin(0, 0, 10, 0x04);   // Phase U' - PA10, TCC0/WO[4], function E
  motor.setLowSidePin(1, 0, 11, 0x04);   // Phase V' - PA11, TCC0/WO[5], function E
  motor.setLowSidePin(2, 0, 12, 0x04);   // Phase W' - PA12, TCC0/WO[6], function E
  
  // Initialize motor
  // begin(num_phases, pole_pairs, pwm_freq_hz, deadtime_ns)
  if (!motor.begin(3, 7, 25000, 1000)) {
    Serial.println("ERROR: Motor initialization failed!");
    while (1);
  }
  
  Serial.println("Motor initialized successfully");
  Serial.println("PWM Frequency: 25 kHz");
  Serial.println("Deadtime: 1000 ns");
  
  // Set commutation mode
  motor.setMode(LQ_BLDC_MODE_SINE);  // Smooth sinusoidal PWM
  
  // Enable motor
  motor.enable(true);
  
  Serial.println("Motor enabled - starting ramp test");
}

void loop() {
  static uint8_t throttle = 0;
  static bool increasing = true;
  static uint32_t last_print = 0;
  
  // Update motor control (call regularly for smooth operation)
  motor.update();
  
  // Ramp throttle up and down
  static uint32_t last_ramp = 0;
  if (millis() - last_ramp >= 50) {  // Update every 50ms
    if (increasing) {
      throttle++;
      if (throttle >= 100) {
        increasing = false;
        Serial.println("Reached max throttle");
      }
    } else {
      throttle--;
      if (throttle == 0) {
        increasing = true;
        Serial.println("Reached min throttle");
      }
    }
    
    motor.setPower(throttle);
    last_ramp = millis();
  }
  
  // Print status every second
  if (millis() - last_print >= 1000) {
    Serial.print("Throttle: ");
    Serial.print(throttle);
    Serial.print("% | Enabled: ");
    Serial.println(motor.isEnabled() ? "Yes" : "No");
    last_print = millis();
  }
  
  // Update at ~1ms for smooth control
  delay(1);
}
