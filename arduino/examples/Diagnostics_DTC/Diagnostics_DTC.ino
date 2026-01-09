/*
 * Diagnostic Trouble Codes (DTC) Example
 * 
 * This example shows how to track and report fault conditions using J1939 DTC format.
 * Useful for automotive, industrial, and safety-critical applications.
 * 
 * Features:
 * - Track active and historical faults
 * - J1939 SPN (Suspect Parameter Number) and FMI (Failure Mode Identifier)
 * - Automatic fault counting and timestamps
 */

#include <LayeredQueue.h>

// DTC manager
struct lq_dtc_manager dtc_mgr;

// Define fault codes (SPN + FMI)
#define SPN_MOTOR_TEMP    110    // Engine coolant temperature
#define SPN_MOTOR_CURRENT 111    // Motor current
#define SPN_SPEED_SENSOR  191    // Speed sensor

#define FMI_ABOVE_NORMAL  3      // Value above normal range
#define FMI_BELOW_NORMAL  4      // Value below normal range
#define FMI_NO_RESPONSE   12     // Component does not respond

// Simulated sensor readings
int16_t motor_temp_c = 25;
uint16_t motor_current_ma = 1000;
bool speed_sensor_ok = true;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  
  Serial.println("LayeredQueue - J1939 DTC Example");
  
  // Initialize DTC manager (1000ms DM1 period)
  lq_dtc_init(&dtc_mgr, 1000);
  
  Serial.println("Fault monitoring active");
  Serial.println("Commands: 's' = show DTCs, 'c' = clear all DTCs");
}

void loop() {
  static uint32_t last_check = 0;
  uint64_t now = millis() * 1000ULL; // Convert to microseconds
  
  // Check for faults every 100ms
  if (millis() - last_check >= 100) {
    last_check = millis();
    
    // Simulate changing conditions
    motor_temp_c = 25 + random(-5, 50);
    motor_current_ma = 1000 + random(0, 4000);
    speed_sensor_ok = (random(0, 100) > 5); // 5% chance of fault
    
    // Check fault conditions and set/clear DTCs
    if (motor_temp_c > 60) {
      lq_dtc_set_active(&dtc_mgr, SPN_MOTOR_TEMP, FMI_ABOVE_NORMAL, LQ_LAMP_AMBER, now);
      Serial.println("⚠️  FAULT: Motor overheating!");
    } else {
      lq_dtc_clear(&dtc_mgr, SPN_MOTOR_TEMP, FMI_ABOVE_NORMAL, now);
    }
    
    if (motor_current_ma > 4000) {
      lq_dtc_set_active(&dtc_mgr, SPN_MOTOR_CURRENT, FMI_ABOVE_NORMAL, LQ_LAMP_AMBER, now);
      Serial.println("⚠️  FAULT: Overcurrent detected!");
    } else {
      lq_dtc_clear(&dtc_mgr, SPN_MOTOR_CURRENT, FMI_ABOVE_NORMAL, now);
    }
    
    if (!speed_sensor_ok) {
      lq_dtc_set_active(&dtc_mgr, SPN_SPEED_SENSOR, FMI_NO_RESPONSE, LQ_LAMP_RED, now);
      Serial.println("⚠️  FAULT: Speed sensor error!");
    } else {
      lq_dtc_clear(&dtc_mgr, SPN_SPEED_SENSOR, FMI_NO_RESPONSE, now);
    }
  }
  
  // Handle serial commands
  if (Serial.available()) {
    char cmd = Serial.read();
    
    if (cmd == 's' || cmd == 'S') {
      // Show all DTCs
      Serial.println("\n=== Diagnostic Trouble Codes ===");
      
      uint8_t active_count = lq_dtc_get_active_count(&dtc_mgr);
      uint8_t stored_count = lq_dtc_get_stored_count(&dtc_mgr);
      
      Serial.print("Active: ");
      Serial.print(active_count);
      Serial.print(" | Stored: ");
      Serial.println(stored_count);
      
      if (active_count + stored_count == 0) {
        Serial.println("No faults recorded ✓");
      } else {
        // Display DTCs
        for (uint8_t i = 0; i < LQ_MAX_DTCS; i++) {
          if (dtc_mgr.dtcs[i].state != LQ_DTC_INACTIVE) {
            Serial.print("SPN ");
            Serial.print(dtc_mgr.dtcs[i].spn);
            Serial.print(" FMI ");
            Serial.print(dtc_mgr.dtcs[i].fmi);
            Serial.print(" - Count: ");
            Serial.print(dtc_mgr.dtcs[i].occurrence_count);
            Serial.print(", State: ");
            switch (dtc_mgr.dtcs[i].state) {
              case LQ_DTC_PENDING: Serial.print("PENDING"); break;
              case LQ_DTC_CONFIRMED: Serial.print("CONFIRMED"); break;
              case LQ_DTC_STORED: Serial.print("STORED"); break;
              default: Serial.print("INACTIVE"); break;
            }
            Serial.print(", Lamp: ");
            switch (dtc_mgr.dtcs[i].lamp) {
              case LQ_LAMP_RED: Serial.println("RED"); break;
              case LQ_LAMP_AMBER: Serial.println("AMBER"); break;
              default: Serial.println("OFF"); break;
            }
          }
        }
      }
      Serial.println("================================\n");
    }
    
    if (cmd == 'c' || cmd == 'C') {
      // Clear all DTCs
      lq_dtc_clear_all(&dtc_mgr);
      Serial.println("✓ All DTCs cleared");
    }
  }
}
