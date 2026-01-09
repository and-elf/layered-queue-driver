/*
 * Diagnostic Trouble Codes (DTC) Example
 * 
 * This example shows how to track and report fault conditions.
 * Useful for automotive, industrial, and safety-critical applications.
 * 
 * Features:
 * - Track active and historical faults
 * - Automatic fault counting and timestamps
 * - Clear codes when conditions are resolved
 */

#include <LayeredQueue.h>

// DTC storage
struct lq_dtc_storage dtc_storage;
struct lq_dtc_entry dtc_buffer[10];

// Define fault codes
#define DTC_OVERHEAT      0x1001  // Motor temperature too high
#define DTC_OVERCURRENT   0x1002  // Motor current too high
#define DTC_SENSOR_FAULT  0x1003  // Speed sensor not responding

// Simulated sensor readings
int16_t motor_temp_c = 25;
uint16_t motor_current_ma = 1000;
bool speed_sensor_ok = true;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  
  Serial.println("LayeredQueue - Diagnostic Trouble Codes Example");
  
  // Initialize DTC storage
  lq_dtc_init(&dtc_storage, dtc_buffer, 10);
  
  Serial.println("Fault monitoring active");
  Serial.println("Commands: 's' = show DTCs, 'c' = clear DTCs");
}

void loop() {
  static uint32_t last_check = 0;
  
  // Check for faults every 100ms
  if (millis() - last_check >= 100) {
    last_check = millis();
    
    // Simulate changing conditions
    motor_temp_c = 25 + random(-5, 50);
    motor_current_ma = 1000 + random(0, 4000);
    speed_sensor_ok = (random(0, 100) > 5); // 5% chance of fault
    
    // Check fault conditions
    if (motor_temp_c > 60) {
      lq_dtc_set(&dtc_storage, DTC_OVERHEAT, true);
      Serial.println("⚠️  FAULT: Motor overheating!");
    } else {
      lq_dtc_set(&dtc_storage, DTC_OVERHEAT, false);
    }
    
    if (motor_current_ma > 4000) {
      lq_dtc_set(&dtc_storage, DTC_OVERCURRENT, true);
      Serial.println("⚠️  FAULT: Overcurrent detected!");
    } else {
      lq_dtc_set(&dtc_storage, DTC_OVERCURRENT, false);
    }
    
    if (!speed_sensor_ok) {
      lq_dtc_set(&dtc_storage, DTC_SENSOR_FAULT, true);
      Serial.println("⚠️  FAULT: Speed sensor error!");
    } else {
      lq_dtc_set(&dtc_storage, DTC_SENSOR_FAULT, false);
    }
  }
  
  // Handle serial commands
  if (Serial.available()) {
    char cmd = Serial.read();
    
    if (cmd == 's' || cmd == 'S') {
      // Show all DTCs
      Serial.println("\n=== Diagnostic Trouble Codes ===");
      
      uint8_t count = lq_dtc_get_count(&dtc_storage);
      if (count == 0) {
        Serial.println("No faults recorded ✓");
      } else {
        for (uint8_t i = 0; i < count; i++) {
          struct lq_dtc_entry *dtc = lq_dtc_get_by_index(&dtc_storage, i);
          if (dtc) {
            Serial.print("Code 0x");
            Serial.print(dtc->code, HEX);
            Serial.print(" - Count: ");
            Serial.print(dtc->occurrence_count);
            Serial.print(", Status: ");
            Serial.println(dtc->is_active ? "ACTIVE" : "Inactive");
          }
        }
      }
      Serial.println("================================\n");
    }
    
    if (cmd == 'c' || cmd == 'C') {
      // Clear all DTCs
      lq_dtc_clear_all(&dtc_storage);
      Serial.println("✓ All DTCs cleared");
    }
  }
  
  delay(10);
}
