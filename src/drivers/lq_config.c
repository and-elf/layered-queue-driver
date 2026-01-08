/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Configuration Framework Implementation
 */

#include "lq_config.h"
#include "lq_uds.h"
#include <string.h>
#include <errno.h>

/* ============================================================================
 * Serialization Helpers
 * ============================================================================ */

/**
 * @brief Serialize remap configuration to bytes
 * 
 * Format: [input_signal][output_signal][flags][deadzone_h][deadzone_l]
 * Total: 5 bytes
 */
static void serialize_remap(const struct lq_remap_ctx *remap, uint8_t *data)
{
    data[0] = remap->input_signal;
    data[1] = remap->output_signal;
    data[2] = (remap->invert ? 0x01 : 0x00) | (remap->enabled ? 0x02 : 0x00);
    data[3] = (uint8_t)(((uint32_t)remap->deadzone >> 8) & 0xFFU);
    data[4] = (uint8_t)((uint32_t)remap->deadzone & 0xFFU);
}

/**
 * @brief Deserialize remap configuration from bytes
 */
static void deserialize_remap(struct lq_remap_ctx *remap, const uint8_t *data)
{
    remap->input_signal = data[0];
    remap->output_signal = data[1];
    remap->invert = (data[2] & 0x01) != 0;
    remap->enabled = (data[2] & 0x02) != 0;
    remap->deadzone = ((int32_t)data[3] << 8) | data[4];
}

/**
 * @brief Serialize scale configuration to bytes
 * 
 * Format: [input_signal][output_signal][flags]
 *         [scale_h][scale_l][offset_h][offset_l]
 *         [clamp_min_hh][clamp_min_hl][clamp_min_lh][clamp_min_ll]
 *         [clamp_max_hh][clamp_max_hl][clamp_max_lh][clamp_max_ll]
 * Total: 19 bytes
 */
static void serialize_scale(const struct lq_scale_ctx *scale, uint8_t *data)
{
    data[0] = scale->input_signal;
    data[1] = scale->output_signal;
    data[2] = (scale->enabled ? 0x01 : 0x00) |
              (scale->has_clamp_min ? 0x02 : 0x00) |
              (scale->has_clamp_max ? 0x04 : 0x00);
    
    /* Scale factor (16-bit signed) */
    data[3] = (uint8_t)(((uint32_t)scale->scale_factor >> 8) & 0xFFU);
    data[4] = (uint8_t)((uint32_t)scale->scale_factor & 0xFFU);
    
    /* Offset (16-bit signed) */
    data[5] = (uint8_t)(((uint32_t)scale->offset >> 8) & 0xFFU);
    data[6] = (uint8_t)((uint32_t)scale->offset & 0xFFU);
    
    /* Clamp min (32-bit signed) */
    data[7] = (uint8_t)(((uint32_t)scale->clamp_min >> 24) & 0xFFU);
    data[8] = (uint8_t)(((uint32_t)scale->clamp_min >> 16) & 0xFFU);
    data[9] = (uint8_t)(((uint32_t)scale->clamp_min >> 8) & 0xFFU);
    data[10] = (uint8_t)((uint32_t)scale->clamp_min & 0xFFU);
    
    /* Clamp max (32-bit signed) */
    data[11] = (uint8_t)(((uint32_t)scale->clamp_max >> 24) & 0xFFU);
    data[12] = (uint8_t)(((uint32_t)scale->clamp_max >> 16) & 0xFFU);
    data[13] = (uint8_t)(((uint32_t)scale->clamp_max >> 8) & 0xFFU);
    data[14] = (uint8_t)((uint32_t)scale->clamp_max & 0xFFU);
}

/**
 * @brief Deserialize scale configuration from bytes
 */
static void deserialize_scale(struct lq_scale_ctx *scale, const uint8_t *data)
{
    scale->input_signal = data[0];
    scale->output_signal = data[1];
    scale->enabled = (data[2] & 0x01) != 0;
    scale->has_clamp_min = (data[2] & 0x02) != 0;
    scale->has_clamp_max = (data[2] & 0x04) != 0;
    
    /* Scale factor (sign-extend 16-bit) */
    int16_t scale_s16 = ((int16_t)data[3] << 8) | data[4];
    scale->scale_factor = scale_s16;
    
    /* Offset (sign-extend 16-bit) */
    int16_t offset_s16 = ((int16_t)data[5] << 8) | data[6];
    scale->offset = offset_s16;
    
    /* Clamp min (32-bit signed) */
    scale->clamp_min = ((int32_t)data[7] << 24) |
                       ((int32_t)data[8] << 16) |
                       ((int32_t)data[9] << 8) |
                       data[10];
    
    /* Clamp max (32-bit signed) */
    scale->clamp_max = ((int32_t)data[11] << 24) |
                       ((int32_t)data[12] << 16) |
                       ((int32_t)data[13] << 8) |
                       data[14];
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int lq_config_init(struct lq_config_registry *registry,
                   struct lq_engine *engine,
                   struct lq_remap_ctx *remaps, uint8_t max_remaps,
                   struct lq_scale_ctx *scales, uint8_t max_scales)
{
    if (!registry || !engine) {
        return -EINVAL;
    }
    
    memset(registry, 0, sizeof(*registry));
    
    registry->engine = engine;
    registry->remaps = remaps;
    registry->max_remaps = max_remaps;
    registry->scales = scales;
    registry->max_scales = max_scales;
    registry->num_remaps = 0;
    registry->num_scales = 0;
    registry->config_locked = false;
    registry->calibration_mode = false;
    registry->config_version = 0;
    
    return 0;
}

int lq_config_read_signal(const struct lq_config_registry *registry,
                          uint8_t signal_id,
                          int32_t *value,
                          uint8_t *status)
{
    if (!registry || !registry->engine || !value || !status) {
        return -EINVAL;
    }
    
    if (signal_id >= LQ_MAX_SIGNALS) {
        return -ENOENT;
    }
    
    *value = registry->engine->signals[signal_id].value;
    *status = (uint8_t)registry->engine->signals[signal_id].status;
    
    return 0;
}

int lq_config_read_remap(const struct lq_config_registry *registry,
                         uint8_t index,
                         struct lq_remap_ctx *remap)
{
    if (!registry || !remap) {
        return -EINVAL;
    }
    
    if (index >= registry->num_remaps) {
        return -ENOENT;
    }
    
    memcpy(remap, &registry->remaps[index], sizeof(*remap));
    return 0;
}

int lq_config_write_remap(struct lq_config_registry *registry,
                          uint8_t index,
                          const struct lq_remap_ctx *remap)
{
    if (!registry || !remap) {
        return -EINVAL;
    }
    
    if (registry->config_locked && !registry->calibration_mode) {
        return -LQ_NRC_SECURITY_ACCESS_DENIED;
    }
    
    if (index >= registry->num_remaps) {
        return -ENOENT;
    }
    
    /* Validate signal IDs */
    if (remap->input_signal >= LQ_MAX_SIGNALS ||
        remap->output_signal >= LQ_MAX_SIGNALS) {
        return -LQ_NRC_REQUEST_OUT_OF_RANGE;
    }
    
    memcpy(&registry->remaps[index], remap, sizeof(*remap));
    registry->config_version++;
    
    return 0;
}

int lq_config_read_scale(const struct lq_config_registry *registry,
                         uint8_t index,
                         struct lq_scale_ctx *scale)
{
    if (!registry || !scale) {
        return -EINVAL;
    }
    
    if (index >= registry->num_scales) {
        return -ENOENT;
    }
    
    memcpy(scale, &registry->scales[index], sizeof(*scale));
    return 0;
}

int lq_config_write_scale(struct lq_config_registry *registry,
                          uint8_t index,
                          const struct lq_scale_ctx *scale)
{
    if (!registry || !scale) {
        return -EINVAL;
    }
    
    if (registry->config_locked && !registry->calibration_mode) {
        return -LQ_NRC_SECURITY_ACCESS_DENIED;
    }
    
    if (index >= registry->num_scales) {
        return -ENOENT;
    }
    
    /* Validate signal IDs */
    if (scale->input_signal >= LQ_MAX_SIGNALS ||
        scale->output_signal >= LQ_MAX_SIGNALS) {
        return -LQ_NRC_REQUEST_OUT_OF_RANGE;
    }
    
    /* Validate clamp range */
    if (scale->has_clamp_min && scale->has_clamp_max) {
        if (scale->clamp_min > scale->clamp_max) {
            return -LQ_NRC_REQUEST_OUT_OF_RANGE;
        }
    }
    
    memcpy(&registry->scales[index], scale, sizeof(*scale));
    registry->config_version++;
    
    return 0;
}

int lq_config_enter_calibration_mode(struct lq_config_registry *registry)
{
    if (!registry) {
        return -EINVAL;
    }
    
    registry->calibration_mode = true;
    return 0;
}

int lq_config_exit_calibration_mode(struct lq_config_registry *registry)
{
    if (!registry) {
        return -EINVAL;
    }
    
    registry->calibration_mode = false;
    registry->config_locked = true;
    
    return 0;
}

int lq_config_reset_to_defaults(struct lq_config_registry *registry)
{
    if (!registry) {
        return -EINVAL;
    }
    
    if (registry->config_locked && !registry->calibration_mode) {
        return -LQ_NRC_SECURITY_ACCESS_DENIED;
    }
    
    /* Clear all configurations */
    if (registry->remaps) {
        memset(registry->remaps, 0, registry->max_remaps * sizeof(struct lq_remap_ctx));
    }
    if (registry->scales) {
        memset(registry->scales, 0, registry->max_scales * sizeof(struct lq_scale_ctx));
    }
    
    registry->num_remaps = 0;
    registry->num_scales = 0;
    registry->config_version++;
    
    return 0;
}

int lq_config_add_remap(struct lq_config_registry *registry,
                        const struct lq_remap_ctx *remap,
                        uint8_t *index)
{
    if (!registry || !remap || !index) {
        return -EINVAL;
    }
    
    if (registry->config_locked && !registry->calibration_mode) {
        return -LQ_NRC_SECURITY_ACCESS_DENIED;
    }
    
    if (registry->num_remaps >= registry->max_remaps) {
        return -ENOMEM;
    }
    
    /* Validate signal IDs */
    if (remap->input_signal >= LQ_MAX_SIGNALS ||
        remap->output_signal >= LQ_MAX_SIGNALS) {
        return -LQ_NRC_REQUEST_OUT_OF_RANGE;
    }
    
    *index = registry->num_remaps;
    memcpy(&registry->remaps[*index], remap, sizeof(*remap));
    registry->num_remaps++;
    registry->config_version++;
    
    return 0;
}

int lq_config_add_scale(struct lq_config_registry *registry,
                        const struct lq_scale_ctx *scale,
                        uint8_t *index)
{
    if (!registry || !scale || !index) {
        return -EINVAL;
    }
    
    if (registry->config_locked && !registry->calibration_mode) {
        return -LQ_NRC_SECURITY_ACCESS_DENIED;
    }
    
    if (registry->num_scales >= registry->max_scales) {
        return -ENOMEM;
    }
    
    /* Validate signal IDs */
    if (scale->input_signal >= LQ_MAX_SIGNALS ||
        scale->output_signal >= LQ_MAX_SIGNALS) {
        return -LQ_NRC_REQUEST_OUT_OF_RANGE;
    }
    
    /* Validate clamp range */
    if (scale->has_clamp_min && scale->has_clamp_max) {
        if (scale->clamp_min > scale->clamp_max) {
            return -LQ_NRC_REQUEST_OUT_OF_RANGE;
        }
    }
    
    *index = registry->num_scales;
    memcpy(&registry->scales[*index], scale, sizeof(*scale));
    registry->num_scales++;
    registry->config_version++;
    
    return 0;
}

int lq_config_remove_remap(struct lq_config_registry *registry,
                           uint8_t index)
{
    if (!registry) {
        return -EINVAL;
    }
    
    if (registry->config_locked && !registry->calibration_mode) {
        return -LQ_NRC_SECURITY_ACCESS_DENIED;
    }
    
    if (index >= registry->num_remaps) {
        return -ENOENT;
    }
    
    /* Shift remaining items down */
    if (index < registry->num_remaps - 1) {
        memmove(&registry->remaps[index],
                &registry->remaps[index + 1],
                (size_t)(registry->num_remaps - index - 1) * sizeof(struct lq_remap_ctx));
    }
    
    registry->num_remaps--;
    registry->config_version++;
    
    return 0;
}

int lq_config_remove_scale(struct lq_config_registry *registry,
                           uint8_t index)
{
    if (!registry) {
        return -EINVAL;
    }
    
    if (registry->config_locked && !registry->calibration_mode) {
        return -LQ_NRC_SECURITY_ACCESS_DENIED;
    }
    
    if (index >= registry->num_scales) {
        return -ENOENT;
    }
    
    /* Shift remaining items down */
    if (index < registry->num_scales - 1) {
        memmove(&registry->scales[index],
                &registry->scales[index + 1],
                (size_t)(registry->num_scales - index - 1) * sizeof(struct lq_scale_ctx));
    }
    
    registry->num_scales--;
    registry->config_version++;
    
    return 0;
}

/* ============================================================================
 * UDS DID Handlers
 * ============================================================================ */

int lq_config_uds_read_did(const struct lq_config_registry *registry,
                           uint16_t did,
                           uint8_t *data,
                           size_t max_len,
                           size_t *actual_len)
{
    if (!registry || !data || !actual_len) {
        return -LQ_NRC_GENERAL_PROGRAMMING_FAILURE;
    }
    
    switch (did) {
    case LQ_DID_SIGNAL_VALUE: {
        /* Format: [signal_id] -> [value_hh][value_hl][value_lh][value_ll] */
        if (max_len < 1) {
            return -LQ_NRC_REQUEST_OUT_OF_RANGE;
        }
        
        uint8_t signal_id = data[0];  /* Signal ID is passed in data[0] */
        int32_t value;
        uint8_t status;
        
        int ret = lq_config_read_signal(registry, signal_id, &value, &status);
        if (ret < 0) {
            return -LQ_NRC_REQUEST_OUT_OF_RANGE;
        }
        
        if (max_len < 4) {
            return -LQ_NRC_GENERAL_PROGRAMMING_FAILURE;
        }
        
        data[0] = (uint8_t)(((uint32_t)value >> 24) & 0xFFU);
        data[1] = (uint8_t)(((uint32_t)value >> 16) & 0xFFU);
        data[2] = (uint8_t)(((uint32_t)value >> 8) & 0xFFU);
        data[3] = (uint8_t)((uint32_t)value & 0xFFU);
        *actual_len = 4;
        return 0;
    }
    
    case LQ_DID_SIGNAL_STATUS: {
        /* Format: [signal_id] -> [status] */
        if (max_len < 1) {
            return -LQ_NRC_REQUEST_OUT_OF_RANGE;
        }
        
        uint8_t signal_id = data[0];
        int32_t value;
        uint8_t status;
        
        int ret = lq_config_read_signal(registry, signal_id, &value, &status);
        if (ret < 0) {
            return -LQ_NRC_REQUEST_OUT_OF_RANGE;
        }
        
        data[0] = status;
        *actual_len = 1;
        return 0;
    }
    
    case LQ_DID_REMAP_CONFIG: {
        /* Format: [index] -> [remap_ctx] */
        if (max_len < 1) {
            return -LQ_NRC_REQUEST_OUT_OF_RANGE;
        }
        
        uint8_t index = data[0];
        struct lq_remap_ctx remap;
        
        int ret = lq_config_read_remap(registry, index, &remap);
        if (ret < 0) {
            return -LQ_NRC_REQUEST_OUT_OF_RANGE;
        }
        
        if (max_len < 5) {
            return -LQ_NRC_GENERAL_PROGRAMMING_FAILURE;
        }
        
        serialize_remap(&remap, data);
        *actual_len = 5;
        return 0;
    }
    
    case LQ_DID_SCALE_CONFIG: {
        /* Format: [index] -> [scale_ctx] */
        if (max_len < 1) {
            return -LQ_NRC_REQUEST_OUT_OF_RANGE;
        }
        
        uint8_t index = data[0];
        struct lq_scale_ctx scale;
        
        int ret = lq_config_read_scale(registry, index, &scale);
        if (ret < 0) {
            return -LQ_NRC_REQUEST_OUT_OF_RANGE;
        }
        
        if (max_len < 15) {
            return -LQ_NRC_GENERAL_PROGRAMMING_FAILURE;
        }
        
        serialize_scale(&scale, data);
        *actual_len = 15;
        return 0;
    }
    
    case LQ_DID_CALIBRATION_MODE: {
        /* Format: [calibration_mode][config_locked][num_remaps][num_scales][version_hh][version_hl][version_lh][version_ll] */
        if (max_len < 8) {
            return -LQ_NRC_GENERAL_PROGRAMMING_FAILURE;
        }
        
        data[0] = registry->calibration_mode ? 1 : 0;
        data[1] = registry->config_locked ? 1 : 0;
        data[2] = registry->num_remaps;
        data[3] = registry->num_scales;
        data[4] = (uint8_t)((registry->config_version >> 24) & 0xFFU);
        data[5] = (uint8_t)((registry->config_version >> 16) & 0xFFU);
        data[6] = (uint8_t)((registry->config_version >> 8) & 0xFFU);
        data[7] = (uint8_t)(registry->config_version & 0xFFU);
        *actual_len = 8;
        return 0;
    }
    
    default:
        return -LQ_NRC_REQUEST_OUT_OF_RANGE;
    }
}

int lq_config_uds_write_did(struct lq_config_registry *registry,
                            uint16_t did,
                            const uint8_t *data,
                            size_t len)
{
    if (!registry || !data) {
        return -LQ_NRC_GENERAL_PROGRAMMING_FAILURE;
    }
    
    switch (did) {
    case LQ_DID_REMAP_CONFIG: {
        /* Format: [index][remap_ctx] */
        if (len < 6) {  /* 1 byte index + 5 bytes remap */
            return -LQ_NRC_INCORRECT_MESSAGE_LENGTH;
        }
        
        uint8_t index = data[0];
        struct lq_remap_ctx remap;
        deserialize_remap(&remap, &data[1]);
        
        int ret = lq_config_write_remap(registry, index, &remap);
        if (ret < 0) {
            return ret;  /* Already a UDS NRC */
        }
        
        return 0;
    }
    
    case LQ_DID_SCALE_CONFIG: {
        /* Format: [index][scale_ctx] */
        if (len < 16) {  /* 1 byte index + 15 bytes scale */
            return -LQ_NRC_INCORRECT_MESSAGE_LENGTH;
        }
        
        uint8_t index = data[0];
        struct lq_scale_ctx scale;
        deserialize_scale(&scale, &data[1]);
        
        int ret = lq_config_write_scale(registry, index, &scale);
        if (ret < 0) {
            return ret;
        }
        
        return 0;
    }
    
    default:
        return -LQ_NRC_REQUEST_OUT_OF_RANGE;
    }
}

int lq_config_uds_routine_control(struct lq_config_registry *registry,
                                  uint16_t rid,
                                  uint8_t control_type,
                                  const uint8_t *in_data,
                                  size_t in_len,
                                  uint8_t *out_data,
                                  size_t max_out,
                                  size_t *actual_out)
{
    if (!registry || !actual_out) {
        return -LQ_NRC_GENERAL_PROGRAMMING_FAILURE;
    }
    
    (void)in_data;  /* Currently unused */
    (void)in_len;
    (void)out_data;
    (void)max_out;
    
    *actual_out = 0;
    
    if (control_type != LQ_UDS_ROUTINE_START) {
        return -LQ_NRC_REQUEST_OUT_OF_RANGE;
    }
    
    switch (rid) {
    case LQ_RID_ENTER_CALIBRATION:
        return lq_config_enter_calibration_mode(registry);
        
    case LQ_RID_EXIT_CALIBRATION:
        return lq_config_exit_calibration_mode(registry);
        
    case LQ_RID_RESET_DEFAULTS:
        return lq_config_reset_to_defaults(registry);
        
    default:
        return -LQ_NRC_REQUEST_OUT_OF_RANGE;
    }
}
