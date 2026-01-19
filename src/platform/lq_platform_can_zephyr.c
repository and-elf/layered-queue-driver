
#include "lq_platform.h"

#ifdef __ZEPHYR__

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/can.h>

// Maximum number of CAN devices supported
#define LQ_MAX_CAN_DEVICES 3
#define LQ_CAN_RX_MSGQ_MAX 8

// CAN device descriptor
struct lq_can_device {
	const struct device *dev;
	struct k_msgq rx_msgq;
	int rx_filter_id;
	bool rx_filter_initialized;
	char msgq_buffer[sizeof(struct can_frame) * LQ_CAN_RX_MSGQ_MAX];
};

// Static storage for CAN devices (can0=0, can1=1, can2=2, etc.)
static struct lq_can_device lq_can_devices[LQ_MAX_CAN_DEVICES];
static bool lq_can_initialized = false;

// Initialize CAN device table on first use
static void lq_can_init(void)
{
	if (lq_can_initialized) {
		return;
	}

	memset(lq_can_devices, 0, sizeof(lq_can_devices));

	// Initialize each CAN device if it exists and is enabled in devicetree
#if DT_NODE_HAS_STATUS(DT_NODELABEL(can0), okay)
	lq_can_devices[0].dev = DEVICE_DT_GET(DT_NODELABEL(can0));
	k_msgq_init(&lq_can_devices[0].rx_msgq, lq_can_devices[0].msgq_buffer,
		    sizeof(struct can_frame), LQ_CAN_RX_MSGQ_MAX);
	lq_can_devices[0].rx_filter_id = -1;
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(can1), okay)
	lq_can_devices[1].dev = DEVICE_DT_GET(DT_NODELABEL(can1));
	k_msgq_init(&lq_can_devices[1].rx_msgq, lq_can_devices[1].msgq_buffer,
		    sizeof(struct can_frame), LQ_CAN_RX_MSGQ_MAX);
	lq_can_devices[1].rx_filter_id = -1;
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(can2), okay)
	lq_can_devices[2].dev = DEVICE_DT_GET(DT_NODELABEL(can2));
	k_msgq_init(&lq_can_devices[2].rx_msgq, lq_can_devices[2].msgq_buffer,
		    sizeof(struct can_frame), LQ_CAN_RX_MSGQ_MAX);
	lq_can_devices[2].rx_filter_id = -1;
#endif

	lq_can_initialized = true;
}

// Get CAN device by index
static struct lq_can_device *lq_can_get_device(uint8_t device_index)
{
	lq_can_init();

	if (device_index >= LQ_MAX_CAN_DEVICES) {
		return NULL;
	}

	struct lq_can_device *can = &lq_can_devices[device_index];
	if (!can->dev || !device_is_ready(can->dev)) {
		return NULL;
	}

	return can;
}

// TX work structure
struct lq_can_tx_work {
	struct k_work work;
	const struct device *dev;
	struct can_frame frame;
	int result;
	struct k_sem done;
};

static void lq_can_tx_work_handler(struct k_work *work)
{
	struct lq_can_tx_work *txw = CONTAINER_OF(work, struct lq_can_tx_work, work);
	int ret = can_send(txw->dev, &txw->frame, K_FOREVER, NULL, NULL);
	txw->result = ret;
	k_sem_give(&txw->done);
}

int lq_can_send(uint8_t device_index, uint32_t can_id, bool is_extended, const uint8_t *data, uint8_t len)
{
	struct lq_can_device *can = lq_can_get_device(device_index);
	if (!can) {
		return -ENODEV;
	}

	struct lq_can_tx_work txw;
	k_sem_init(&txw.done, 0, 1);
	k_work_init(&txw.work, lq_can_tx_work_handler);
	txw.dev = can->dev;
	memset(&txw.frame, 0, sizeof(txw.frame));
	txw.frame.id = can_id;
	txw.frame.dlc = len > 8 ? 8 : len;
	memcpy(txw.frame.data, data, txw.frame.dlc);
	txw.frame.flags = is_extended ? CAN_FRAME_IDE : 0;

	k_work_submit(&txw.work);
	k_sem_take(&txw.done, K_FOREVER);
	return txw.result == 0 ? 0 : -EIO;
}

// RX filter setup (per device)
static int lq_can_init_rx_filter(struct lq_can_device *can)
{
	if (can->rx_filter_initialized) {
		return 0;
	}

	struct can_filter filter = {
		.id = 0,
		.mask = 0,
		.flags = 0, // Accept all
	};
	can->rx_filter_id = can_add_rx_filter_msgq(can->dev, &can->rx_msgq, &filter);
	if (can->rx_filter_id < 0) {
		return can->rx_filter_id;
	}
	can->rx_filter_initialized = true;
	return 0;
}

int lq_can_recv(uint8_t device_index, uint32_t *can_id, bool *is_extended, uint8_t *data, uint8_t *len, uint32_t timeout_ms)
{
	struct lq_can_device *can = lq_can_get_device(device_index);
	if (!can) {
		return -ENODEV;
	}

	if (!can->rx_filter_initialized) {
		int ret = lq_can_init_rx_filter(can);
		if (ret < 0) {
			return ret;
		}
	}

	if (!can_id || !is_extended || !data || !len) {
		return -EINVAL;
	}

	struct can_frame frame;
	k_timeout_t timeout;
	if (timeout_ms == 0) {
		timeout = K_NO_WAIT;
	} else if (timeout_ms == UINT32_MAX) {
		timeout = K_FOREVER;
	} else {
		timeout = K_MSEC(timeout_ms);
	}

	int ret = k_msgq_get(&can->rx_msgq, &frame, timeout);
	if (ret != 0) {
		return -EAGAIN;
	}

	*can_id = frame.id;
	*is_extended = (frame.flags & CAN_FRAME_IDE) != 0;
	*len = frame.dlc;
	memcpy(data, frame.data, frame.dlc);
	return 0;
}

#endif /* __ZEPHYR__ */