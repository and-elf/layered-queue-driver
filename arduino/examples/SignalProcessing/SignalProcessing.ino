/*
 * Signal Processing Example
 * 
 * This example shows how to use the signal processing features:
 * - Scale: Convert sensor readings (e.g., 0-1023 ADC to 0-100%)
 * - Remap: Apply custom curves and lookup tables
 * - Filters: Smooth noisy sensor data
 * 
 * Common use case: Process throttle input before applying to motor
 */

#include <LayeredQueue.h>

// Signal processing engine
struct lq_engine engine;

// Processing blocks
struct lq_scale_ctx throttle_scaler;
struct lq_remap_ctx throttle_curve;

// Signal IDs
#define SIG_RAW_ADC        0  // Raw ADC reading (0-1023)
#define SIG_THROTTLE_PCT   1  // Scaled to percentage (0-1000)
#define SIG_THROTTLE_CURVE 2  // After applying custom curve

// Custom throttle curve (gives better low-speed control)
// Input: 0-1000, Output: 0-1000
int32_t curve_points[][2] = {
  {0,   0},    // 0% input → 0% output
  {250, 100},  // 25% input → 10% output (gentle start)
  {500, 400},  // 50% input → 40% output
  {750, 700},  // 75% input → 70% output
  {1000, 1000} // 100% input → 100% output
};

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  
  Serial.println("LayeredQueue - Signal Processing Example");
  
  // Initialize engine
  lq_engine_init(&engine);
  engine.num_signals = 10;
  
  // Configure scaler (ADC 0-1023 → percentage 0-1000)
  throttle_scaler.input_signal = SIG_RAW_ADC;
  throttle_scaler.output_signal = SIG_THROTTLE_PCT;
  throttle_scaler.input_min = 0;
  throttle_scaler.input_max = 1023;
  throttle_scaler.output_min = 0;
  throttle_scaler.output_max = 1000;
  throttle_scaler.clamp = true;
  
  lq_scale_init(&throttle_scaler);
  
  // Configure remap (apply custom curve)
  throttle_curve.input_signal = SIG_THROTTLE_PCT;
  throttle_curve.output_signal = SIG_THROTTLE_CURVE;
  throttle_curve.num_points = 5;
  
  // Copy curve points
  for (int i = 0; i < 5; i++) {
    throttle_curve.map_points[i].input = curve_points[i][0];
    throttle_curve.map_points[i].output = curve_points[i][1];
  }
  
  lq_remap_init(&throttle_curve);
  
  Serial.println("Signal processing configured!");
  Serial.println("Reading analog input A0...");
}

void loop() {
  static uint32_t last_update = 0;
  
  if (millis() - last_update >= 100) {
    last_update = millis();
    
    // Read analog input (simulated - use real analogRead(A0) in your project)
    int32_t adc_value = random(0, 1024);
    lq_engine_set_signal(&engine, SIG_RAW_ADC, adc_value);
    
    // Process signals
    uint64_t now = micros();
    lq_scale_update(&throttle_scaler, &engine, now);
    lq_remap_update(&throttle_curve, &engine, now);
    
    // Get results
    int32_t raw = lq_engine_get_signal(&engine, SIG_RAW_ADC);
    int32_t scaled = lq_engine_get_signal(&engine, SIG_THROTTLE_PCT);
    int32_t curved = lq_engine_get_signal(&engine, SIG_THROTTLE_CURVE);
    
    // Print results
    Serial.print("ADC: ");
    Serial.print(raw);
    Serial.print(" → Scaled: ");
    Serial.print(scaled / 10.0, 1);
    Serial.print("% → Curved: ");
    Serial.print(curved / 10.0, 1);
    Serial.println("%");
  }
}
