/*
 * J1939 Protocol Implementation
 */

#include "lq_j1939.h"
#include <string.h>

int lq_j1939_format_dm1(const lq_j1939_dm1_t *dm1, uint8_t *data, size_t data_len)
{
    if (!dm1 || !data || data_len < 8) {
        return -1;
    }
    
    memset(data, 0xFF, data_len);  /* J1939 default: 0xFF = not available */
    
    /* Byte 0-1: Lamp status */
    data[0] = (dm1->protect_lamp & 0x03) |
              ((dm1->amber_warning_lamp & 0x03) << 2) |
              ((dm1->red_stop_lamp & 0x03) << 4) |
              ((dm1->malfunction_lamp & 0x03) << 6);
    
    data[1] = (dm1->flash_malfunction_lamp & 0x03) |
              ((dm1->flash_red_stop_lamp & 0x03) << 2) |
              ((dm1->flash_amber_warning_lamp & 0x03) << 4) |
              ((dm1->flash_protect_lamp & 0x03) << 6);
    
    /* Bytes 2-7: First DTC (if any) */
    if (dm1->dtc_count > 0) {
        uint32_t dtc = dm1->dtc_list[0];
        uint32_t spn = lq_j1939_get_spn(dtc);
        uint8_t fmi = lq_j1939_get_fmi(dtc);
        uint8_t oc = lq_j1939_get_oc(dtc);
        
        /* SPN: 19 bits split across bytes 2-4 */
        data[2] = spn & 0xFF;
        data[3] = (spn >> 8) & 0xFF;
        data[4] = ((spn >> 16) & 0x07) | ((fmi & 0x1F) << 3);
        data[5] = ((fmi >> 5) & 0x03) | (oc << 2);
        
        /* Conversion method (always 0 for J1939) */
        data[6] = 0;
        data[7] = 0;
    }
    
    return 0;
}

int lq_j1939_format_dm0(lq_j1939_lamp_t stop_lamp, lq_j1939_lamp_t warning_lamp,
                        uint8_t *data, size_t data_len)
{
    if (!data || data_len < 8) {
        return -1;
    }
    
    memset(data, 0xFF, data_len);
    
    /* DM0 format similar to DM1 but without DTCs */
    data[0] = ((stop_lamp & 0x03) << 4) |
              ((warning_lamp & 0x03) << 2);
    
    data[1] = 0xFF;  /* No flash patterns */
    
    return 0;
}
