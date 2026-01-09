/*
 * CANopen SDO (Service Data Object) Example
 * 
 * This example demonstrates CANopen communication for industrial automation.
 * Shows how to read/write parameters from CANopen devices.
 * 
 * CANopen is widely used in:
 * - Industrial motor controllers
 * - Robotics
 * - Factory automation
 */

#include <LayeredQueue.h>

// CANopen node ID
#define NODE_ID 0x05

// Common CANopen objects
#define OBJ_DEVICE_TYPE      0x1000  // Device type
#define OBJ_ERROR_REGISTER   0x1001  // Error status
#define OBJ_HEARTBEAT_TIME   0x1017  // Heartbeat interval
#define OBJ_MOTOR_SPEED      0x6042  // Target velocity

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  
  Serial.println("LayeredQueue - CANopen SDO Example");
  
  // Initialize your CAN library here
  // e.g., CAN.begin(125000); // 125 kbps typical for CANopen
  
  Serial.println("Ready to send CANopen messages");
}

void loop() {
  static uint32_t last_heartbeat = 0;
  uint8_t msg_buffer[8];
  
  // Send heartbeat every 1000ms
  if (millis() - last_heartbeat >= 1000) {
    last_heartbeat = millis();
    
    // Encode CANopen heartbeat message
    lq_canopen_encode_heartbeat(msg_buffer, 0x05); // State: Operational
    
    Serial.println("Heartbeat sent");
    // Send via CAN: ID = 0x700 + NODE_ID
    // CAN.beginPacket(0x700 + NODE_ID, 1, false);
    // CAN.write(msg_buffer, 1);
    // CAN.endPacket();
  }
  
  // Example: Read device type from remote node
  // This would be triggered by user input or startup sequence
  static bool read_sent = false;
  if (millis() > 2000 && !read_sent) {
    read_sent = true;
    
    // Encode SDO read request for object 0x1000 (Device Type)
    lq_canopen_encode_sdo_read(msg_buffer, OBJ_DEVICE_TYPE, 0x00);
    
    Serial.println("Requesting device type from node...");
    // Send via CAN: ID = 0x600 + NODE_ID (SDO request)
    // CAN.beginPacket(0x600 + NODE_ID, 8, false);
    // CAN.write(msg_buffer, 8);
    // CAN.endPacket();
  }
  
  // Example: Write motor speed setpoint
  static bool write_sent = false;
  if (millis() > 3000 && !write_sent) {
    write_sent = true;
    
    int32_t target_rpm = 1500;
    
    // Encode SDO write request for object 0x6042 (Target velocity)
    lq_canopen_encode_sdo_write(msg_buffer, OBJ_MOTOR_SPEED, 0x00, 
                                (uint8_t*)&target_rpm, 4);
    
    Serial.print("Setting motor speed to ");
    Serial.print(target_rpm);
    Serial.println(" RPM");
    // Send via CAN
    // CAN.beginPacket(0x600 + NODE_ID, 8, false);
    // CAN.write(msg_buffer, 8);
    // CAN.endPacket();
  }
  
  delay(10);
}
