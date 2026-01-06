/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * DTC Management Implementation
 */

#include "lq_dtc.h"
#include <string.h>

/* Helper: Find DTC by SPN and FMI */
static struct lq_dtc *find_dtc(struct lq_dtc_manager *mgr, uint32_t spn, uint8_t fmi)
{
    for (int i = 0; i < LQ_MAX_DTCS; i++) {
        struct lq_dtc *dtc = &mgr->dtcs[i];
        if (dtc->state != LQ_DTC_INACTIVE && dtc->spn == spn && dtc->fmi == fmi) {
            return dtc;
        }
    }
    return NULL;
}

/* Helper: Find free slot */
static struct lq_dtc *find_free_slot(struct lq_dtc_manager *mgr)
{
    for (int i = 0; i < LQ_MAX_DTCS; i++) {
        if (mgr->dtcs[i].state == LQ_DTC_INACTIVE) {
            return &mgr->dtcs[i];
        }
    }
    return NULL;
}

void lq_dtc_init(struct lq_dtc_manager *mgr, uint32_t dm1_period_ms)
{
    memset(mgr, 0, sizeof(*mgr));
    mgr->dm1_period_ms = dm1_period_ms ? dm1_period_ms : 1000;
}

int lq_dtc_set_active(struct lq_dtc_manager *mgr, uint32_t spn, uint8_t fmi,
                      enum lq_lamp_status lamp, uint64_t now)
{
    /* Check if DTC already exists */
    struct lq_dtc *dtc = find_dtc(mgr, spn, fmi);
    
    if (dtc) {
        /* Update existing DTC */
        if (dtc->state == LQ_DTC_STORED) {
            /* Re-activating a stored fault */
            mgr->num_stored--;
            mgr->num_active++;
        }
        dtc->state = LQ_DTC_CONFIRMED;
        dtc->last_active_us = now;
        dtc->occurrence_count++;
        if (dtc->occurrence_count == 0) {
            dtc->occurrence_count = 255;  /* Saturate at 255 */
        }
        /* Update lamp to more severe if needed */
        if (lamp > dtc->lamp) {
            dtc->lamp = lamp;
        }
    } else {
        /* Create new DTC */
        dtc = find_free_slot(mgr);
        if (!dtc) {
            return -12;  /* -ENOMEM */
        }
        
        dtc->spn = spn;
        dtc->fmi = fmi;
        dtc->state = LQ_DTC_CONFIRMED;
        dtc->lamp = lamp;
        dtc->occurrence_count = 1;
        dtc->first_detected_us = now;
        dtc->last_active_us = now;
        
        mgr->num_active++;
    }
    
    return 0;
}

int lq_dtc_clear(struct lq_dtc_manager *mgr, uint32_t spn, uint8_t fmi, uint64_t now)
{
    struct lq_dtc *dtc = find_dtc(mgr, spn, fmi);
    if (!dtc) {
        return -2;  /* -ENOENT */
    }
    
    if (dtc->state == LQ_DTC_CONFIRMED) {
        mgr->num_active--;
    }
    
    /* Move to stored state for history tracking */
    dtc->state = LQ_DTC_STORED;
    dtc->lamp = LQ_LAMP_OFF;
    mgr->num_stored++;
    
    (void)now;  /* Could use for aging logic */
    
    return 0;
}

void lq_dtc_clear_all(struct lq_dtc_manager *mgr)
{
    memset(mgr->dtcs, 0, sizeof(mgr->dtcs));
    mgr->num_active = 0;
    mgr->num_stored = 0;
}

uint8_t lq_dtc_get_active_count(const struct lq_dtc_manager *mgr)
{
    return mgr->num_active;
}

uint8_t lq_dtc_get_stored_count(const struct lq_dtc_manager *mgr)
{
    return mgr->num_stored;
}

enum lq_lamp_status lq_dtc_get_mil_status(const struct lq_dtc_manager *mgr)
{
    enum lq_lamp_status highest = LQ_LAMP_OFF;
    
    for (int i = 0; i < LQ_MAX_DTCS; i++) {
        const struct lq_dtc *dtc = &mgr->dtcs[i];
        if (dtc->state == LQ_DTC_CONFIRMED && dtc->lamp > highest) {
            highest = dtc->lamp;
        }
    }
    
    return highest;
}

int lq_dtc_build_dm1(struct lq_dtc_manager *mgr, uint8_t *data, 
                     size_t max_size, uint64_t now)
{
    /* Rate limit DM1 broadcasts */
    uint64_t elapsed_us = now - mgr->last_dm1_send_us;
    uint64_t period_us = (uint64_t)mgr->dm1_period_ms * 1000;
    
    if (mgr->last_dm1_send_us != 0 && elapsed_us < period_us) {
        return 0;  /* Not time to send yet */
    }
    
    if (max_size < 8) {
        return -22;  /* -EINVAL - need at least 8 bytes */
    }
    
    /* Get highest severity lamp */
    enum lq_lamp_status mil = lq_dtc_get_mil_status(mgr);
    
    /* Byte 0-1: Lamp status (J1939 format) */
    data[0] = (mil == LQ_LAMP_AMBER || mil == LQ_LAMP_AMBER_FLASH) ? 0xC0 : 0x00;  /* Protect lamp */
    data[1] = (mil == LQ_LAMP_RED) ? 0xC0 : 0x00;  /* Amber warning lamp */
    data[2] = (mil == LQ_LAMP_RED) ? 0xC0 : 0x00;  /* Red stop lamp */
    data[3] = (mil == LQ_LAMP_AMBER_FLASH) ? 0xC0 : 0x00;  /* Flash bits */
    
    /* Bytes 4-7: Reserved/padding for first DTC or 0xFF if none */
    size_t offset = 4;
    int dtc_count = 0;
    
    /* Encode active DTCs (4 bytes each) */
    for (int i = 0; i < LQ_MAX_DTCS && offset + 4 <= max_size; i++) {
        const struct lq_dtc *dtc = &mgr->dtcs[i];
        
        if (dtc->state != LQ_DTC_CONFIRMED) {
            continue;
        }
        
        /* DTC format: [SPN_low, SPN_mid, SPN_high(3 bits) << 5 | FMI(5 bits), occurrence] */
        data[offset++] = dtc->spn & 0xFF;
        data[offset++] = (dtc->spn >> 8) & 0xFF;
        data[offset++] = ((dtc->spn >> 16) << 5) | (dtc->fmi & 0x1F);
        data[offset++] = dtc->occurrence_count;
        
        dtc_count++;
    }
    
    /* If no DTCs, fill with 0xFF as per J1939 spec */
    if (dtc_count == 0) {
        data[4] = 0xFF;
        data[5] = 0xFF;
        data[6] = 0xFF;
        data[7] = 0xFF;
        offset = 8;
    }
    
    mgr->last_dm1_send_us = now;
    
    return (int)offset;
}

int lq_dtc_build_dm2(struct lq_dtc_manager *mgr, uint8_t *data, size_t max_size)
{
    if (max_size < 8) {
        return -22;  /* -EINVAL */
    }
    
    /* DM2 has no lamps (all off for previously active) */
    data[0] = 0x00;
    data[1] = 0x00;
    data[2] = 0x00;
    data[3] = 0x00;
    
    size_t offset = 4;
    int dtc_count = 0;
    
    /* Encode stored DTCs */
    for (int i = 0; i < LQ_MAX_DTCS && offset + 4 <= max_size; i++) {
        const struct lq_dtc *dtc = &mgr->dtcs[i];
        
        if (dtc->state != LQ_DTC_STORED) {
            continue;
        }
        
        data[offset++] = dtc->spn & 0xFF;
        data[offset++] = (dtc->spn >> 8) & 0xFF;
        data[offset++] = ((dtc->spn >> 16) << 5) | (dtc->fmi & 0x1F);
        data[offset++] = dtc->occurrence_count;
        
        dtc_count++;
    }
    
    if (dtc_count == 0) {
        data[4] = 0xFF;
        data[5] = 0xFF;
        data[6] = 0xFF;
        data[7] = 0xFF;
        offset = 8;
    }
    
    return (int)offset;
}
