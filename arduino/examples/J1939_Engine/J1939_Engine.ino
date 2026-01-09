/*
 * J1939 Engine Data Example
 * 
 * This example shows how to format J1939 CAN messages for automotive/industrial applications.
 * Broadcasts engine speed, torque, and temperature on CAN bus.
 * 
 * Requires: CAN transceiver connected to your Arduino board
 * Libraries: Use with arduino-CAN, MCP2515, etc.
 */

#include <LayeredQueue.h>

// J1939 message buffers
uint8_t eec1_msg[8];  // Electronic Engine Controller 1
uint8_t et1_msg[8];   // Engine Temperature 1

// Simulated engine data
uint16_t engine_speed_rpm = 0;
uint8_t engine_torque_pct = 0;
int16_t coolant_temp_c = 20;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  
  Serial.println("LayeredQueue - J1939 Engine Data Example");
  Serial.println("Broadcasting engine data...");
  
  // Initialize your CAN library here
  // e.g., CAN.begin(250000); // 250 kbps for J1939
}

void loop() {
  static uint32_t last_eec1 = 0;
  static uint32_t last_et1 = 0;
  
  // Send EEC1 message every 50ms (20 Hz - typical for engine speed)
  if (millis() - last_eec1 >= 50) {
    last_eec1 = millis();
    
    // Simulate engine running
    engine_speed_rpm = 800 + (millis() / 100) % 3000; // Ramp 800-3800 RPM
    engine_torque_pct = 45;
    
    // Encode J1939 EEC1 message
    // Parameters: buffer, speed, torque, driver_demand, actual_pct, source_addr
    lq_j1939_encode_eec1(eec1_msg, engine_speed_rpm, engine_torque_pct, 
                         50, 48, 0x00);
    
    // Send on CAN bus (uncomment when you have CAN hardware)
    // CAN.beginPacket(0x0CF00400, 8, false); // PGN 61444, extended ID
    // CAN.write(eec1_msg, 8);
    // CAN.endPacket();
    
    // Print for debugging
    Serial.print("EEC1: Speed=");
    Serial.print(engine_speed_rpm);
    Serial.print(" RPM, Torque=");
    Serial.print(engine_torque_pct);
    Serial.println("%");
  }
  
  // Send ET1 message every 1000ms (1 Hz - typical for temperature)
  if (millis() - last_et1 >= 1000) {
    last_et1 = millis();
    
    // Simulate temperature changing
    coolant_temp_c = 20 + (millis() / 10000) % 80; // 20-100°C
    
    // Encode J1939 ET1 message
    lq_j1939_encode_et1(et1_msg, coolant_temp_c, -40, 80, 30);
    
    // Send on CAN bus (uncomment when you have CAN hardware)
    // CAN.beginPacket(0x0CFEF200, 8, false); // PGN 65266
    // CAN.write(et1_msg, 8);
    // CAN.endPacket();
    
    Serial.print("ET1: Coolant=");
    Serial.print(coolant_temp_c);
    Serial.println("°C");
  }
}
