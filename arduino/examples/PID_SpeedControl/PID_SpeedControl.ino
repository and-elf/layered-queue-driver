/*
 * PID Speed Control Example
 * 
 * This example demonstrates closed-loop motor speed control using a PID controller.
 * Requires a motor with speed feedback (hall sensors, encoder, or back-EMF).
 * 
 * The PID controller maintains constant speed even under varying load.
 */

#include <LayeredQueue.h>

// Engine for signal processing
struct lq_engine engine;

// PID controller
struct lq_pid_ctx speed_pid;

// Motor
struct lq_bldc_config motor_config;
struct lq_bldc_ctx motor;

// Signal IDs
#define SIG_SPEED_SETPOINT  0
#define SIG_SPEED_MEASURED  1
#define SIG_THROTTLE_OUTPUT 2

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  
  Serial.println("LayeredQueue - PID Speed Control Example");
  
  // Initialize signal processing engine
  lq_engine_init(&engine);
  engine.num_signals = 10;
  
  // Configure PID controller
  speed_pid.setpoint_signal = SIG_SPEED_SETPOINT;
  speed_pid.measurement_signal = SIG_SPEED_MEASURED;
  speed_pid.output_signal = SIG_THROTTLE_OUTPUT;
  
  // PID gains (scaled by 1000)
  speed_pid.kp = 500;   // Proportional: 0.5
  speed_pid.ki = 100;   // Integral: 0.1
  speed_pid.kd = 50;    // Derivative: 0.05
  
  // Output limits (throttle 0-1000)
  speed_pid.output_min = 0;
  speed_pid.output_max = 1000;
  
  // Integral anti-windup
  speed_pid.integral_min = -10000;
  speed_pid.integral_max = 10000;
  
  speed_pid.enabled = true;
  
  lq_pid_init(&speed_pid);
  
  // Initialize motor
  motor_config.pole_pairs = 7;
  motor_config.pwm_frequency = 20000;
  motor_config.deadtime_ns = 500;
  motor_config.max_duty = 950;
  
  if (lq_bldc_init(&motor, &motor_config) != 0) {
    Serial.println("ERROR: Failed to initialize motor!");
    while (1) { delay(100); }
  }
  
  // Set desired speed (1500 RPM)
  lq_engine_set_signal(&engine, SIG_SPEED_SETPOINT, 1500);
  
  Serial.println("PID controller active - maintaining 1500 RPM");
}

void loop() {
  static uint32_t last_print = 0;
  uint64_t now = micros();
  
  // Simulate speed measurement (replace with real sensor reading)
  // In real application: read encoder, hall sensors, or back-EMF
  int32_t measured_speed = 1450 + random(-50, 50); // Simulated noise
  lq_engine_set_signal(&engine, SIG_SPEED_MEASURED, measured_speed);
  
  // Run PID controller
  lq_pid_update(&speed_pid, &engine, now);
  
  // Get PID output and apply to motor
  int32_t throttle = lq_engine_get_signal(&engine, SIG_THROTTLE_OUTPUT);
  lq_bldc_set_throttle(&motor, throttle);
  
  // Update motor
  lq_bldc_update(&motor, now);
  
  // Print status every 500ms
  if (millis() - last_print >= 500) {
    last_print = millis();
    
    int32_t setpoint = lq_engine_get_signal(&engine, SIG_SPEED_SETPOINT);
    
    Serial.print("Setpoint: ");
    Serial.print(setpoint);
    Serial.print(" RPM | Measured: ");
    Serial.print(measured_speed);
    Serial.print(" RPM | Throttle: ");
    Serial.print(throttle / 10.0, 1);
    Serial.println("%");
  }
}
