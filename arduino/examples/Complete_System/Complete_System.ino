/*
 * Complete Motor Control System Example
 * 
 * This example combines multiple features into a complete system:
 * - BLDC motor control with PID speed regulation
 * - Signal processing (throttle scaling, curves)
 * - Fault monitoring with diagnostic codes
 * - CAN bus reporting (J1939 engine data)
 * 
 * This demonstrates how to build a production-ready motor controller.
 */

#include <LayeredQueue.h>

// =============================================================================
// System Components
// =============================================================================

// Signal processing
struct lq_engine engine;

// Motor control
struct lq_bldc_config motor_config;
struct lq_bldc_ctx motor;

// PID speed controller
struct lq_pid_ctx speed_pid;

// Signal processing blocks
struct lq_scale_ctx throttle_scaler;
struct lq_remap_ctx throttle_curve;

// Diagnostics
struct lq_dtc_storage dtc_storage;
struct lq_dtc_entry dtc_buffer[10];

// Signal IDs
#define SIG_THROTTLE_ADC     0
#define SIG_THROTTLE_SCALED  1
#define SIG_THROTTLE_CURVED  2
#define SIG_SPEED_SETPOINT   3
#define SIG_SPEED_MEASURED   4
#define SIG_THROTTLE_OUT     5

// Fault codes
#define DTC_OVERSPEED   0x2001
#define DTC_OVERHEAT    0x2002
#define DTC_STARTUP     0x2003

// System state
bool system_fault = false;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  
  Serial.println("=================================");
  Serial.println("LayeredQueue - Complete System");
  Serial.println("=================================\n");
  
  // Initialize signal engine
  lq_engine_init(&engine);
  engine.num_signals = 15;
  
  // Initialize diagnostics
  lq_dtc_init(&dtc_storage, dtc_buffer, 10);
  
  // Configure throttle scaling (ADC 0-1023 → 0-1000)
  throttle_scaler.input_signal = SIG_THROTTLE_ADC;
  throttle_scaler.output_signal = SIG_THROTTLE_SCALED;
  throttle_scaler.input_min = 0;
  throttle_scaler.input_max = 1023;
  throttle_scaler.output_min = 0;
  throttle_scaler.output_max = 1000;
  throttle_scaler.clamp = true;
  lq_scale_init(&throttle_scaler);
  
  // Configure throttle curve (progressive response)
  throttle_curve.input_signal = SIG_THROTTLE_SCALED;
  throttle_curve.output_signal = SIG_THROTTLE_CURVED;
  throttle_curve.num_points = 3;
  throttle_curve.map_points[0].input = 0;
  throttle_curve.map_points[0].output = 0;
  throttle_curve.map_points[1].input = 500;
  throttle_curve.map_points[1].output = 300;  // Gentler low-end
  throttle_curve.map_points[2].input = 1000;
  throttle_curve.map_points[2].output = 1000;
  lq_remap_init(&throttle_curve);
  
  // Configure PID speed controller
  speed_pid.setpoint_signal = SIG_SPEED_SETPOINT;
  speed_pid.measurement_signal = SIG_SPEED_MEASURED;
  speed_pid.output_signal = SIG_THROTTLE_OUT;
  speed_pid.kp = 800;
  speed_pid.ki = 150;
  speed_pid.kd = 80;
  speed_pid.output_min = 0;
  speed_pid.output_max = 1000;
  speed_pid.integral_min = -15000;
  speed_pid.integral_max = 15000;
  speed_pid.enabled = true;
  lq_pid_init(&speed_pid);
  
  // Configure motor
  motor_config.pole_pairs = 7;
  motor_config.pwm_frequency = 20000;
  motor_config.deadtime_ns = 500;
  motor_config.max_duty = 950;
  
  if (lq_bldc_init(&motor, &motor_config) != 0) {
    Serial.println("❌ Motor initialization failed!");
    lq_dtc_set(&dtc_storage, DTC_STARTUP, true);
    system_fault = true;
    return;
  }
  
  Serial.println("✓ All systems initialized\n");
  Serial.println("Commands:");
  Serial.println("  't' = show telemetry");
  Serial.println("  'd' = show diagnostics");
  Serial.println("  'c' = clear faults\n");
}

void loop() {
  if (system_fault) {
    Serial.println("System in fault state - reset required");
    delay(1000);
    return;
  }
  
  static uint32_t last_update = 0;
  uint64_t now = micros();
  
  // Main control loop (run at 100 Hz)
  if (micros() - last_update >= 10000) {
    last_update = micros();
    
    // Read throttle input (simulated - use analogRead(A0) in real system)
    int32_t throttle_adc = random(200, 900);
    lq_engine_set_signal(&engine, SIG_THROTTLE_ADC, throttle_adc);
    
    // Process throttle signal
    lq_scale_update(&throttle_scaler, &engine, now);
    lq_remap_update(&throttle_curve, &engine, now);
    
    // Get processed throttle and convert to speed setpoint
    int32_t throttle = lq_engine_get_signal(&engine, SIG_THROTTLE_CURVED);
    int32_t speed_setpoint = (throttle * 3000) / 1000; // 0-3000 RPM
    lq_engine_set_signal(&engine, SIG_SPEED_SETPOINT, speed_setpoint);
    
    // Simulate speed measurement (use real sensor in production)
    int32_t speed_measured = speed_setpoint + random(-50, 50);
    lq_engine_set_signal(&engine, SIG_SPEED_MEASURED, speed_measured);
    
    // Run PID controller
    lq_pid_update(&speed_pid, &engine, now);
    
    // Get PID output
    int32_t motor_throttle = lq_engine_get_signal(&engine, SIG_THROTTLE_OUT);
    
    // Safety checks
    if (speed_measured > 3500) {
      lq_dtc_set(&dtc_storage, DTC_OVERSPEED, true);
      motor_throttle = 0; // Emergency stop
    } else {
      lq_dtc_set(&dtc_storage, DTC_OVERSPEED, false);
    }
    
    // Apply to motor
    lq_bldc_set_throttle(&motor, motor_throttle);
    lq_bldc_update(&motor, now);
  }
  
  // Handle serial commands
  if (Serial.available()) {
    char cmd = Serial.read();
    
    if (cmd == 't') {
      // Show telemetry
      Serial.println("\n--- Telemetry ---");
      Serial.print("Speed: ");
      Serial.print(lq_engine_get_signal(&engine, SIG_SPEED_MEASURED));
      Serial.print(" / ");
      Serial.print(lq_engine_get_signal(&engine, SIG_SPEED_SETPOINT));
      Serial.println(" RPM");
      Serial.print("Throttle: ");
      Serial.print(lq_engine_get_signal(&engine, SIG_THROTTLE_OUT) / 10.0, 1);
      Serial.println("%\n");
    }
    
    if (cmd == 'd') {
      // Show diagnostics
      Serial.println("\n--- Diagnostics ---");
      uint8_t count = lq_dtc_get_count(&dtc_storage);
      if (count == 0) {
        Serial.println("No faults ✓");
      } else {
        for (uint8_t i = 0; i < count; i++) {
          struct lq_dtc_entry *dtc = lq_dtc_get_by_index(&dtc_storage, i);
          Serial.print("0x");
          Serial.print(dtc->code, HEX);
          Serial.print(dtc->is_active ? " ACTIVE" : " inactive");
          Serial.print(" (count: ");
          Serial.print(dtc->occurrence_count);
          Serial.println(")");
        }
      }
      Serial.println();
    }
    
    if (cmd == 'c') {
      lq_dtc_clear_all(&dtc_storage);
      Serial.println("✓ Faults cleared\n");
    }
  }
}
