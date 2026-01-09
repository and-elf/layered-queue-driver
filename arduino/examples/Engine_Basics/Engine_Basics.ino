/*
 * Engine Basics - Signal Routing
 * 
 * This example demonstrates the core of LayeredQueue: the signal processing engine.
 * The engine routes data between different processing blocks using numbered signals.
 * 
 * Think of it like a patch panel - you connect inputs to outputs, and data flows through.
 * 
 * Key concepts:
 * - Signals: Named data slots (like variables)
 * - Helper functions: lq_engine_set_signal() and lq_engine_get_signal()
 * - Engine: Manages signal storage
 */

#include <LayeredQueue.h>

// The engine manages signal flow
struct lq_engine engine;

// Signal IDs (just names for signal slots)
#define SIG_SENSOR_RAW      0
#define SIG_SENSOR_FILTERED 1
#define SIG_SETPOINT        2
#define SIG_ERROR           3
#define SIG_OUTPUT          4

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  
  Serial.println("LayeredQueue - Engine Basics");
  Serial.println("=============================\n");
  
  // Initialize engine
  lq_engine_init(&engine);
  engine.num_signals = 10;
  
  Serial.println("Engine initialized with 10 signal slots\n");
  
  // Set initial values
  lq_engine_set_signal(&engine, SIG_SETPOINT, 500);
  
  Serial.println("Demo: Simple signal flow");
  Serial.println("-------------------------");
  Serial.println("Sensor → Filter → Error Calculation → Output");
  Serial.println("Commands: '+' increase setpoint, '-' decrease, 'd' dump signals\n");
}

void loop() {
  static uint32_t last_update = 0;
  
  if (millis() - last_update >= 500) {
    last_update = millis();
    
    // =========================================================================
    // Step 1: Read sensor (simulated - use analogRead() in real project)
    // =========================================================================
    int32_t sensor_raw = 400 + random(-50, 100);  // Noisy sensor
    lq_engine_set_signal(&engine, SIG_SENSOR_RAW, sensor_raw);
    
    Serial.print("1. Sensor reading: ");
    Serial.println(sensor_raw);
    
    // =========================================================================
    // Step 2: Simple filtering (moving average)
    // =========================================================================
    static int32_t filter_buffer[4] = {0, 0, 0, 0};
    static uint8_t filter_index = 0;
    
    filter_buffer[filter_index] = sensor_raw;
    filter_index = (filter_index + 1) % 4;
    
    int32_t sensor_filtered = (filter_buffer[0] + filter_buffer[1] + 
                               filter_buffer[2] + filter_buffer[3]) / 4;
    
    lq_engine_set_signal(&engine, SIG_SENSOR_FILTERED, sensor_filtered);
    
    Serial.print("2. After filter: ");
    Serial.println(sensor_filtered);
    
    // =========================================================================
    // Step 3: Calculate error (setpoint - measurement)
    // =========================================================================
    int32_t setpoint = lq_engine_get_signal(&engine, SIG_SETPOINT);
    int32_t error = setpoint - sensor_filtered;
    
    lq_engine_set_signal(&engine, SIG_ERROR, error);
    
    Serial.print("3. Error (");
    Serial.print(setpoint);
    Serial.print(" - ");
    Serial.print(sensor_filtered);
    Serial.print("): ");
    Serial.println(error);
    
    // =========================================================================
    // Step 4: Simple proportional control
    // =========================================================================
    int32_t output = error * 2;  // Proportional gain of 2
    
    // Clamp output
    if (output > 1000) output = 1000;
    if (output < -1000) output = -1000;
    
    lq_engine_set_signal(&engine, SIG_OUTPUT, output);
    
    Serial.print("4. Control output: ");
    Serial.println(output);
    
    Serial.println("---");
  }
  
  // Handle commands
  if (Serial.available()) {
    char cmd = Serial.read();
    
    if (cmd == '+') {
      int32_t current = lq_engine_get_signal(&engine, SIG_SETPOINT);
      lq_engine_set_signal(&engine, SIG_SETPOINT, current + 50);
      Serial.print("\nSetpoint increased to: ");
      Serial.println(current + 50);
      Serial.println();
    }
    
    if (cmd == '-') {
      int32_t current = lq_engine_get_signal(&engine, SIG_SETPOINT);
      lq_engine_set_signal(&engine, SIG_SETPOINT, current - 50);
      Serial.print("\nSetpoint decreased to: ");
      Serial.println(current - 50);
      Serial.println();
    }
    
    if (cmd == 'd') {
      // Dump all signals
      Serial.println("\n=== Signal Dump ===");
      Serial.print("Sensor Raw:      ");
      Serial.println(lq_engine_get_signal(&engine, SIG_SENSOR_RAW));
      Serial.print("Sensor Filtered: ");
      Serial.println(lq_engine_get_signal(&engine, SIG_SENSOR_FILTERED));
      Serial.print("Setpoint:        ");
      Serial.println(lq_engine_get_signal(&engine, SIG_SETPOINT));
      Serial.print("Error:           ");
      Serial.println(lq_engine_get_signal(&engine, SIG_ERROR));
      Serial.print("Output:          ");
      Serial.println(lq_engine_get_signal(&engine, SIG_OUTPUT));
      Serial.println("==================\n");
    }
  }
}
