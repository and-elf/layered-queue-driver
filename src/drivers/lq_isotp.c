/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * ISO-TP (ISO 15765-2) Implementation
 */

#include "lq_isotp.h"
#include "lq_platform.h"
#include <string.h>
#include <errno.h>

/* Helper macros */
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* ISO-TP frame layout */
#define SF_MAX_DATA_LEN     7   /* Single frame max payload */
#define FF_MAX_DATA_LEN     6   /* First frame max payload */
#define CF_MAX_DATA_LEN     7   /* Consecutive frame max payload */

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint8_t get_pci_type(const uint8_t *data)
{
    return data[0] & 0xF0;
}

static void send_single_frame(struct lq_isotp_channel *ch,
                              lq_isotp_can_send_fn can_send, void *can_ctx)
{
    struct lq_isotp_can_frame frame = {
        .can_id = ch->config.tx_id,
        .is_extended = ch->config.use_extended_id,
        .dlc = (uint8_t)(ch->tx_len + 1),
    };
    
    /* PCI: 0x0N where N is length */
    frame.data[0] = (uint8_t)(ISOTP_PCI_SF | (ch->tx_len & 0x0F));
    memcpy(&frame.data[1], ch->tx_data, ch->tx_len);
    
    can_send(can_ctx, &frame);
    
    ch->tx_state = ISOTP_TX_IDLE;
    ch->tx_data = NULL;
}

static void send_first_frame(struct lq_isotp_channel *ch,
                             lq_isotp_can_send_fn can_send, void *can_ctx,
                             uint64_t now)
{
    struct lq_isotp_can_frame frame = {
        .can_id = ch->config.tx_id,
        .is_extended = ch->config.use_extended_id,
        .dlc = 8,
    };
    
    /* PCI: 0x1L LL where LLL is 12-bit length */
    frame.data[0] = (uint8_t)(ISOTP_PCI_FF | ((ch->tx_len >> 8) & 0x0F));
    frame.data[1] = (uint8_t)(ch->tx_len & 0xFF);
    
    memcpy(&frame.data[2], ch->tx_data, FF_MAX_DATA_LEN);
    
    can_send(can_ctx, &frame);
    
    ch->tx_offset = FF_MAX_DATA_LEN;
    ch->tx_sn = 1;  /* Next consecutive frame SN */
    ch->tx_state = ISOTP_TX_WAIT_FC;
    ch->tx_deadline = now + (ch->config.n_bs_timeout * 1000ULL);
}

static void send_consecutive_frame(struct lq_isotp_channel *ch,
                                    lq_isotp_can_send_fn can_send, void *can_ctx,
                                    uint64_t now)
{
    struct lq_isotp_can_frame frame = {
        .can_id = ch->config.tx_id,
        .is_extended = ch->config.use_extended_id,
    };
    
    size_t remaining = ch->tx_len - ch->tx_offset;
    size_t to_send = MIN(remaining, CF_MAX_DATA_LEN);
    
    /* PCI: 0x2N where N is sequence number */
    frame.data[0] = (uint8_t)(ISOTP_PCI_CF | (ch->tx_sn & 0x0F));
    memcpy(&frame.data[1], ch->tx_data + ch->tx_offset, to_send);
    
    frame.dlc = (uint8_t)(to_send + 1);
    
    can_send(can_ctx, &frame);
    
    ch->tx_offset += to_send;
    ch->tx_sn = (ch->tx_sn + 1) & 0x0F;
    ch->block_count++;
    
    /* Check if done */
    if (ch->tx_offset >= ch->tx_len) {
        ch->tx_state = ISOTP_TX_IDLE;
        ch->tx_data = NULL;
    } else if (ch->fc_bs > 0 && ch->block_count >= ch->fc_bs) {
        /* Need to wait for next FC */
        ch->tx_state = ISOTP_TX_WAIT_FC;
        ch->tx_deadline = now + (ch->config.n_bs_timeout * 1000ULL);
        ch->block_count = 0;
    }
}

static void send_flow_control(struct lq_isotp_channel *ch,
                               uint8_t fs, uint8_t bs, uint8_t st_min,
                               lq_isotp_can_send_fn can_send, void *can_ctx)
{
    struct lq_isotp_can_frame frame = {
        .can_id = ch->config.tx_id,
        .is_extended = ch->config.use_extended_id,
        .dlc = 3,
    };
    
    frame.data[0] = (uint8_t)(ISOTP_PCI_FC | (fs & 0x0F));
    frame.data[1] = bs;
    frame.data[2] = st_min;
    
    can_send(can_ctx, &frame);
}

static int handle_single_frame(struct lq_isotp_channel *ch,
                                const struct lq_isotp_can_frame *frame,
                                lq_isotp_can_send_fn can_send, void *can_ctx)
{
    (void)can_send;
    (void)can_ctx;
    
    uint8_t len = frame->data[0] & 0x0F;
    
    if (len == 0 || len > SF_MAX_DATA_LEN) {
        return -EINVAL;
    }
    
    if (len > ch->rx_buf_size) {
        return -ENOMEM;
    }
    
    /* Copy data */
    memcpy(ch->rx_buf, &frame->data[1], len);
    ch->rx_len = len;
    ch->rx_offset = len;
    ch->rx_state = ISOTP_RX_IDLE;
    
    return 0;
}

static int handle_first_frame(struct lq_isotp_channel *ch,
                               const struct lq_isotp_can_frame *frame,
                               lq_isotp_can_send_fn can_send, void *can_ctx,
                               uint64_t now)
{
    /* Extract length (12 bits) */
    uint16_t len = (uint16_t)(((frame->data[0] & 0x0F) << 8) | frame->data[1]);
    
    if (len <= SF_MAX_DATA_LEN || len > ch->rx_buf_size) {
        /* Invalid length or buffer too small - send overflow */
        send_flow_control(ch, ISOTP_FC_OVFLW, 0, 0, can_send, can_ctx);
        ch->rx_state = ISOTP_RX_IDLE;
        return 0;  /* Overflow sent successfully */
    }
    
    /* Copy first frame data */
    memcpy(ch->rx_buf, &frame->data[2], FF_MAX_DATA_LEN);
    ch->rx_len = len;
    ch->rx_offset = FF_MAX_DATA_LEN;
    ch->rx_sn = 1;  /* Expect CF with SN=1 */
    ch->rx_state = ISOTP_RX_RECEIVING_CF;
    ch->rx_deadline = now + (ch->config.n_cr * 1000ULL);
    
    /* Send flow control - CTS */
    send_flow_control(ch, ISOTP_FC_CTS, 
                      (uint8_t)ch->config.n_bs,
                      (uint8_t)ch->config.n_st_min,
                      can_send, can_ctx);
    
    return 0;
}

static int handle_consecutive_frame(struct lq_isotp_channel *ch,
                                     const struct lq_isotp_can_frame *frame,
                                     uint64_t now)
{
    if (ch->rx_state != ISOTP_RX_RECEIVING_CF) {
        return -EINVAL;
    }
    
    uint8_t sn = frame->data[0] & 0x0F;
    
    /* Check sequence number */
    if (sn != ch->rx_sn) {
        ch->rx_state = ISOTP_RX_IDLE;
        return -EINVAL;
    }
    
    /* Copy data */
    size_t remaining = ch->rx_len - ch->rx_offset;
    size_t to_copy = MIN(remaining, CF_MAX_DATA_LEN);
    
    memcpy(ch->rx_buf + ch->rx_offset, &frame->data[1], to_copy);
    ch->rx_offset += to_copy;
    
    /* Update sequence number */
    ch->rx_sn = (ch->rx_sn + 1) & 0x0F;
    
    /* Update timeout */
    ch->rx_deadline = now + (ch->config.n_cr * 1000ULL);
    
    /* Check if complete */
    if (ch->rx_offset >= ch->rx_len) {
        ch->rx_state = ISOTP_RX_IDLE;
    }
    
    return 0;
}

static int handle_flow_control(struct lq_isotp_channel *ch,
                                const struct lq_isotp_can_frame *frame,
                                uint64_t now)
{
    if (ch->tx_state != ISOTP_TX_WAIT_FC) {
        return 0;  /* Ignore unexpected FC */
    }
    
    uint8_t fs = frame->data[0] & 0x0F;
    uint8_t bs = frame->data[1];
    uint8_t st_min = frame->data[2];
    
    if (fs == ISOTP_FC_CTS) {
        /* Continue sending */
        ch->fc_bs = bs;
        ch->fc_st_min = st_min;
        ch->block_count = 0;
        ch->tx_state = ISOTP_TX_SENDING_CF;
        
        /* Calculate next CF time based on STmin */
        uint64_t st_min_us;
        if (st_min <= 127) {
            st_min_us = st_min * 1000ULL;  /* 0-127 ms */
        } else if (st_min >= 0xF1 && st_min <= 0xF9) {
            st_min_us = (st_min - 0xF0) * 100ULL;  /* 100-900 µs */
        } else {
            st_min_us = 0;
        }
        
        ch->next_cf_time = now + st_min_us;
        
    } else if (fs == ISOTP_FC_WAIT) {
        /* Keep waiting */
        ch->tx_deadline = now + (ch->config.n_bs_timeout * 1000ULL);
        
    } else if (fs == ISOTP_FC_OVFLW) {
        /* Receiver overflow - abort */
        ch->tx_state = ISOTP_TX_IDLE;
        ch->tx_data = NULL;
        return 0;  /* Abort successful */
    }
    
    return 0;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int lq_isotp_init(struct lq_isotp_channel *channel,
                  const struct lq_isotp_config *config,
                  uint8_t *rx_buffer)
{
    if (!channel || !config || !rx_buffer) {
        return -EINVAL;
    }
    
    memset(channel, 0, sizeof(*channel));
    memcpy(&channel->config, config, sizeof(*config));
    
    /* Set default timing if not configured */
    if (channel->config.n_as == 0) {
        channel->config.n_as = 1000;  /* 1 second */
    }
    if (channel->config.n_ar == 0) {
        channel->config.n_ar = 1000;
    }
    if (channel->config.n_bs_timeout == 0) {
        channel->config.n_bs_timeout = 1000;
    }
    if (channel->config.n_cr == 0) {
        channel->config.n_cr = 1000;
    }
    if (channel->config.tx_buf_size == 0) {
        channel->config.tx_buf_size = 4095;
    }
    if (channel->config.rx_buf_size == 0) {
        channel->config.rx_buf_size = 4095;
    }
    
    channel->rx_buf = rx_buffer;
    channel->rx_buf_size = channel->config.rx_buf_size;
    
    return 0;
}

int lq_isotp_send(struct lq_isotp_channel *channel,
                  const uint8_t *data, size_t len,
                  lq_isotp_can_send_fn can_send, void *can_ctx,
                  uint64_t now)
{
    if (!channel || !data || len == 0 || !can_send) {
        return -EINVAL;
    }
    
    if (channel->tx_state != ISOTP_TX_IDLE) {
        return -EBUSY;
    }
    
    if (len > channel->config.tx_buf_size) {
        return -EMSGSIZE;
    }
    
    channel->tx_data = data;
    channel->tx_len = len;
    channel->tx_offset = 0;
    
    if (len <= SF_MAX_DATA_LEN) {
        /* Single frame */
        send_single_frame(channel, can_send, can_ctx);
    } else {
        /* Multi-frame - start with FF */
        send_first_frame(channel, can_send, can_ctx, now);
    }
    
    return 0;
}

int lq_isotp_recv(struct lq_isotp_channel *channel,
                  const struct lq_isotp_can_frame *frame,
                  lq_isotp_can_send_fn can_send, void *can_ctx,
                  uint64_t now)
{
    if (!channel || !frame) {
        return -EINVAL;
    }
    
    /* Ignore frames not for this channel */
    if (frame->can_id != channel->config.rx_id) {
        return 0;
    }
    
    uint8_t pci_type = get_pci_type(frame->data);
    
    switch (pci_type) {
        case ISOTP_PCI_SF:
            return handle_single_frame(channel, frame, can_send, can_ctx);
            
        case ISOTP_PCI_FF:
            return handle_first_frame(channel, frame, can_send, can_ctx, now);
            
        case ISOTP_PCI_CF:
            return handle_consecutive_frame(channel, frame, now);
            
        case ISOTP_PCI_FC:
            return handle_flow_control(channel, frame, now);
            
        default:
            return -EINVAL;
    }
}

int lq_isotp_periodic(struct lq_isotp_channel *channel,
                      lq_isotp_can_send_fn can_send, void *can_ctx,
                      uint64_t now)
{
    if (!channel) {
        return -EINVAL;
    }
    
    /* Check TX timeouts */
    if (channel->tx_state == ISOTP_TX_WAIT_FC) {
        if (now >= channel->tx_deadline) {
            /* Timeout waiting for FC */
            channel->tx_state = ISOTP_TX_IDLE;
            channel->tx_data = NULL;
            return -ETIMEDOUT;
        }
    }
    
    /* Send consecutive frames if ready */
    if (channel->tx_state == ISOTP_TX_SENDING_CF) {
        if (now >= channel->next_cf_time) {
            send_consecutive_frame(channel, can_send, can_ctx, now);
            
            /* Update next CF time */
            if (channel->tx_state == ISOTP_TX_SENDING_CF) {
                uint64_t st_min_us;
                if (channel->fc_st_min <= 127) {
                    st_min_us = channel->fc_st_min * 1000ULL;
                } else if (channel->fc_st_min >= 0xF1 && channel->fc_st_min <= 0xF9) {
                    st_min_us = (channel->fc_st_min - 0xF0) * 100ULL;
                } else {
                    st_min_us = 0;
                }
                channel->next_cf_time = now + st_min_us;
            }
        }
    }
    
    /* Check RX timeouts */
    if (channel->rx_state == ISOTP_RX_RECEIVING_CF) {
        if (now >= channel->rx_deadline) {
            /* Timeout waiting for CF - clear reception */
            channel->rx_state = ISOTP_RX_IDLE;
            channel->rx_offset = 0;
            channel->rx_len = 0;
            return -ETIMEDOUT;
        }
    }
    
    return 0;
}

bool lq_isotp_tx_done(const struct lq_isotp_channel *channel)
{
    return channel ? (channel->tx_state == ISOTP_TX_IDLE) : true;
}

bool lq_isotp_rx_available(struct lq_isotp_channel *channel,
                           const uint8_t **data, size_t *len)
{
    if (!channel || !data || !len) {
        return false;
    }
    
    if (channel->rx_state == ISOTP_RX_IDLE && channel->rx_offset > 0) {
        *data = channel->rx_buf;
        *len = channel->rx_len;
        return true;
    }
    
    return false;
}

void lq_isotp_rx_ack(struct lq_isotp_channel *channel)
{
    if (channel) {
        channel->rx_offset = 0;
        channel->rx_len = 0;
    }
}

void lq_isotp_abort(struct lq_isotp_channel *channel)
{
    if (channel) {
        channel->tx_state = ISOTP_TX_IDLE;
        channel->tx_data = NULL;
        channel->rx_state = ISOTP_RX_IDLE;
        channel->rx_offset = 0;
        channel->rx_len = 0;
    }
}
