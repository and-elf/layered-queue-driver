/*
 * Simple PID Control Example
 * 
 * Demonstrates closed-loop control with a PID controller.
 * Maintains a setpoint using feedback.
 */

#include <LayeredQueue.h>

struct lq_engine engine;

// Signal IDs
#define SIG_SETPOINT   0
#define SIG_MEASUREMENT 1
#define SIG_OUTPUT     2

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  
  Serial.println("LayeredQueue - Simple PID Example");
  
  // Initialize engine
  lq_engine_init(&engine);
  engine.num_signals = 5;
  
  // Configure PID controller
  engine.pids[0].setpoint_signal = SIG_SETPOINT;
  engine.pids[0].measurement_signal = SIG_MEASUREMENT;
  engine.pids[0].output_signal = SIG_OUTPUT;
  engine.pids[0].kp = 500;  // 0.5
  engine.pids[0].ki = 100;  // 0.1
  engine.pids[0].kd = 50;   // 0.05
  engine.pids[0].output_min = 0;
  engine.pids[0].output_max = 1000;
  engine.pids[0].integral_min = -10000;
  engine.pids[0].integral_max = 10000;
  engine.pids[0].deadband = 0;
  engine.pids[0].sample_time_us = 0;
  engine.pids[0].reset_on_setpoint_change = false;
  engine.pids[0].enabled = true;
  engine.pids[0].first_run = true;
  engine.num_pids = 1;
  
  // Set target
  lq_engine_set_signal(&engine, SIG_SETPOINT, 500);
  
  Serial.println("PID controller ready!");
}

void loop() {
  static uint32_t last_update = 0;
  
  if (millis() - last_update >= 100) {
    last_update = millis();
    uint64_t now = micros();
    
    // Simulate measurement with noise
    int32_t setpoint = lq_engine_get_signal(&engine, SIG_SETPOINT);
    int32_t measurement = setpoint + random(-50, 50);
    lq_engine_set_signal(&engine, SIG_MEASUREMENT, measurement);
    
    // Process PID
    lq_process_pids(&engine, engine.pids, engine.num_pids, now);
    
    // Get output
    int32_t output = lq_engine_get_signal(&engine, SIG_OUTPUT);
    
    Serial.print("Setpoint: ");
    Serial.print(setpoint);
    Serial.print(" | Meas: ");
    Serial.print(measurement);
    Serial.print(" | Output: ");
    Serial.println(output);
  }
}
