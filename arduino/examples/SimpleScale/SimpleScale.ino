/*
 * Simple Scaling Example
 * 
 * Shows how to use the scale driver to convert sensor readings.
 * 
 * Example: ADC (0-1023) → Percentage (0-1000)
 */

#include <LayeredQueue.h>

struct lq_engine engine;

// Signal IDs
#define SIG_ADC_RAW    0
#define SIG_PERCENTAGE 1

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  
  Serial.println("LayeredQueue - Simple Scaling Example");
  
  // Initialize engine
  lq_engine_init(&engine);
  engine.num_signals = 5;
  
  // Add a scale driver to the engine
  engine.scales[0].input_signal = SIG_ADC_RAW;
  engine.scales[0].output_signal = SIG_PERCENTAGE;
  engine.scales[0].scale_factor = 977;  // 1023 → 1000 (977/1000)
  engine.scales[0].offset = 0;
  engine.scales[0].clamp_min = 0;
  engine.scales[0].clamp_max = 1000;
  engine.scales[0].has_clamp_min = true;
  engine.scales[0].has_clamp_max = true;
  engine.scales[0].enabled = true;
  engine.num_scales = 1;
  
  Serial.println("Scale driver configured!");
}

void loop() {
  static uint32_t last_update = 0;
  
  if (millis() - last_update >= 200) {
    last_update = millis();
    uint64_t now = micros();
    
    // Read analog input (simulated)
    int32_t adc = random(0, 1024);
    lq_engine_set_signal(&engine, SIG_ADC_RAW, adc);
    
    // Process scale driver
    lq_process_scales(&engine, engine.scales, engine.num_scales, now);
    
    // Read result
    int32_t percentage = lq_engine_get_signal(&engine, SIG_PERCENTAGE);
    
    Serial.print("ADC: ");
    Serial.print(adc);
    Serial.print(" → ");
    Serial.print(percentage / 10.0, 1);
    Serial.println("%");
  }
}
