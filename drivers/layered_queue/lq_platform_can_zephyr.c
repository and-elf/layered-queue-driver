/*
 * Zephyr CAN platform driver for generalized lq,input/lq,output model
 * Supports both protocol-specific and raw CAN input/output nodes
 *
 * This file should be included in the Zephyr build for CAN hardware support.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/sys/printk.h>
#include "lq_platform.h"
#include "lq_hw_input.h"

// TODO: Replace with generated mapping if available
typedef struct {
    const struct device *can_dev;
    uint32_t can_id;
    bool is_extended;
} lq_can_output_map_t;

// Example: single CAN output mapping (should be generated)
static lq_can_output_map_t lq_can_outputs[] = {
    { .can_dev = DEVICE_DT_GET(DT_NODELABEL(can1)), .can_id = 0x123, .is_extended = false },
};

#define NUM_CAN_OUTPUTS (sizeof(lq_can_outputs)/sizeof(lq_can_outputs[0]))

int lq_can_send(uint32_t can_id, bool is_extended, const uint8_t *data, uint8_t len)
{
    for (size_t i = 0; i < NUM_CAN_OUTPUTS; ++i) {
        if (lq_can_outputs[i].can_id == can_id && lq_can_outputs[i].is_extended == is_extended) {
            struct zcan_frame frame = {
                .id = can_id,
                .dlc = len,
                .rtr = 0,
                .ide = is_extended,
            };
            memcpy(frame.data, data, len > 8 ? 8 : len);
            int ret = can_send(lq_can_outputs[i].can_dev, &frame, K_MSEC(10), NULL, NULL);
            if (ret != 0) {
                printk("CAN send failed: %d\n", ret);
            }
            return ret;
        }
    }
    printk("lq_can_send: No mapping for CAN ID 0x%x (ext=%d)\n", can_id, is_extended);
    return -ENODEV;
}

// TODO: Implement lq_can_recv and input mapping for raw CAN input nodes
