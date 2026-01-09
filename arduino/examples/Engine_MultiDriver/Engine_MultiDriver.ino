/*
 * Engine Multi-Driver Example
 * 
 * This example shows the real power of the engine: connecting multiple drivers
 * that automatically process signals in a pipeline.
 * 
 * Pipeline: Raw Input → Scale → Remap → PID → Motor
 * 
 * Each driver:
 * 1. Reads from input signal(s)
 * 2. Processes data
 * 3. Writes to output signal(s)
 * 
 * The engine coordinates everything!
 */

#include <LayeredQueue.h>

// Engine and signal storage
struct lq_engine engine;

// Processing blocks (drivers)
struct lq_scale_ctx input_scaler;
struct lq_remap_ctx throttle_curve;
struct lq_scale_ctx rpm_scaler;
struct lq_pid_ctx speed_controller;

// Signal IDs - named for clarity
#define SIG_JOYSTICK_ADC     0   // Raw joystick (0-1023)
#define SIG_JOYSTICK_PCT     1   // Scaled to percentage (0-1000)
#define SIG_JOYSTICK_CURVED  2   // After custom curve
#define SIG_SPEED_TARGET     3   // Target RPM (0-3000)
#define SIG_SPEED_MEASURED   4   // Actual RPM from sensor
#define SIG_MOTOR_THROTTLE   5   // Final throttle (0-1000)

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  
  Serial.println("LayeredQueue - Multi-Driver Engine Example");
  Serial.println("===========================================\n");
  
  // =========================================================================
  // Initialize Engine
  // =========================================================================
  lq_engine_init(&engine);
  engine.num_signals = 20;
  Serial.println("✓ Engine initialized");
  
  // =========================================================================
  // Configure Driver 1: Scale joystick ADC to percentage
  // =========================================================================
  input_scaler.input_signal = SIG_JOYSTICK_ADC;
  input_scaler.output_signal = SIG_JOYSTICK_PCT;
  input_scaler.input_min = 0;
  input_scaler.input_max = 1023;
  input_scaler.output_min = 0;
  input_scaler.output_max = 1000;
  input_scaler.clamp = true;
  lq_scale_init(&input_scaler);
  Serial.println("✓ Scaler configured (ADC → Percentage)");
  
  // =========================================================================
  // Configure Driver 2: Apply custom throttle curve
  // =========================================================================
  throttle_curve.input_signal = SIG_JOYSTICK_PCT;
  throttle_curve.output_signal = SIG_JOYSTICK_CURVED;
  throttle_curve.num_points = 4;
  throttle_curve.map_points[0].input = 0;
  throttle_curve.map_points[0].output = 0;
  throttle_curve.map_points[1].input = 300;    // Gentle start
  throttle_curve.map_points[1].output = 100;
  throttle_curve.map_points[2].input = 700;
  throttle_curve.map_points[2].output = 600;
  throttle_curve.map_points[3].input = 1000;
  throttle_curve.map_points[3].output = 1000;
  lq_remap_init(&throttle_curve);
  Serial.println("✓ Remap configured (Progressive curve)");
  
  // =========================================================================
  // Configure Driver 3: Convert percentage to RPM target
  // =========================================================================
  rpm_scaler.input_signal = SIG_JOYSTICK_CURVED;
  rpm_scaler.output_signal = SIG_SPEED_TARGET;
  rpm_scaler.input_min = 0;
  rpm_scaler.input_max = 1000;
  rpm_scaler.output_min = 0;
  rpm_scaler.output_max = 3000;  // Max 3000 RPM
  rpm_scaler.clamp = true;
  lq_scale_init(&rpm_scaler);
  Serial.println("✓ RPM scaler configured (Pct → RPM)");
  
  // =========================================================================
  // Configure Driver 4: PID speed controller
  // =========================================================================
  speed_controller.setpoint_signal = SIG_SPEED_TARGET;
  speed_controller.measurement_signal = SIG_SPEED_MEASURED;
  speed_controller.output_signal = SIG_MOTOR_THROTTLE;
  speed_controller.kp = 600;
  speed_controller.ki = 120;
  speed_controller.kd = 60;
  speed_controller.output_min = 0;
  speed_controller.output_max = 1000;
  speed_controller.integral_min = -10000;
  speed_controller.integral_max = 10000;
  speed_controller.enabled = true;
  lq_pid_init(&speed_controller);
  Serial.println("✓ PID controller configured");
  
  Serial.println("\n=== Signal Flow ===");
  Serial.println("Joystick ADC (0) → Scaler → Percentage (1)");
  Serial.println("                            ↓");
  Serial.println("                         Remap → Curved (2)");
  Serial.println("                                    ↓");
  Serial.println("                         RPM Scale → Target RPM (3)");
  Serial.println("                                    ↓");
  Serial.println("                            PID ← Measured RPM (4)");
  Serial.println("                             ↓");
  Serial.println("                      Motor Throttle (5)");
  Serial.println("===================\n");
  
  Serial.println("Reading joystick on A0...");
  Serial.println("Commands: 'd' = dump all signals\n");
}

void loop() {
  static uint32_t last_update = 0;
  
  if (millis() - last_update >= 100) {
    last_update = millis();
    uint64_t now = micros();
    
    // =========================================================================
    // Input: Read joystick (simulated - use analogRead(A0) in real project)
    // =========================================================================
    int32_t joystick = random(200, 900);
    lq_engine_set_signal(&engine, SIG_JOYSTICK_ADC, joystick);
    
    // Simulate motor speed feedback
    int32_t target = lq_engine_get_signal(&engine, SIG_SPEED_TARGET);
    int32_t measured = target + random(-100, 100);  // Simulated noise
    lq_engine_set_signal(&engine, SIG_SPEED_MEASURED, measured);
    
    // =========================================================================
    // Update All Drivers - The engine routes signals automatically!
    // =========================================================================
    lq_scale_update(&input_scaler, &engine, now);        // ADC → Pct
    lq_remap_update(&throttle_curve, &engine, now);      // Pct → Curved
    lq_scale_update(&rpm_scaler, &engine, now);          // Curved → RPM
    lq_pid_update(&speed_controller, &engine, now);      // PID control
    
    // =========================================================================
    // Read final output
    // =========================================================================
    int32_t throttle = lq_engine_get_signal(&engine, SIG_MOTOR_THROTTLE);
    
    // Print compact status
    Serial.print("Joy: ");
    Serial.print(joystick);
    Serial.print(" → ");
    Serial.print(lq_engine_get_signal(&engine, SIG_JOYSTICK_PCT) / 10.0, 1);
    Serial.print("% → ");
    Serial.print(lq_engine_get_signal(&engine, SIG_JOYSTICK_CURVED) / 10.0, 1);
    Serial.print("% → ");
    Serial.print(target);
    Serial.print(" RPM → Throttle: ");
    Serial.print(throttle / 10.0, 1);
    Serial.println("%");
  }
  
  // Dump all signals on command
  if (Serial.available() && Serial.read() == 'd') {
    Serial.println("\n=== SIGNAL DUMP ===");
    Serial.print("[0] Joystick ADC:    ");
    Serial.println(lq_engine_get_signal(&engine, SIG_JOYSTICK_ADC));
    Serial.print("[1] Joystick Pct:    ");
    Serial.print(lq_engine_get_signal(&engine, SIG_JOYSTICK_PCT) / 10.0, 1);
    Serial.println("%");
    Serial.print("[2] Joystick Curved: ");
    Serial.print(lq_engine_get_signal(&engine, SIG_JOYSTICK_CURVED) / 10.0, 1);
    Serial.println("%");
    Serial.print("[3] Speed Target:    ");
    Serial.print(lq_engine_get_signal(&engine, SIG_SPEED_TARGET));
    Serial.println(" RPM");
    Serial.print("[4] Speed Measured:  ");
    Serial.print(lq_engine_get_signal(&engine, SIG_SPEED_MEASURED));
    Serial.println(" RPM");
    Serial.print("[5] Motor Throttle:  ");
    Serial.print(lq_engine_get_signal(&engine, SIG_MOTOR_THROTTLE) / 10.0, 1);
    Serial.println("%");
    Serial.println("===================\n");
  }
}
