/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Arduino Library Main Header
 * 
 * This single header includes all available functionality.
 * Individual components can be included separately if needed.
 */

#ifndef LAYEREDQUEUE_H_
#define LAYEREDQUEUE_H_

/* Include all headers from parent repository */
#include "../../include/lq_common.h"
#include "../../include/lq_platform.h"
#include "../../include/layered_queue_core.h"
#include "../../include/lq_event.h"
#include "../../include/lq_util.h"

/* Core drivers */
#include "../../include/lq_engine.h"
#include "../../include/lq_hw_input.h"
#include "../../include/lq_mid_driver.h"

/* Signal processing */
#include "../../include/lq_scale.h"
#include "../../include/lq_remap.h"
#include "../../include/lq_pid.h"
#include "../../include/lq_verified_output.h"

/* Motor control */
#include "../../include/lq_bldc.h"

/* Communication protocols */
#include "../../include/lq_j1939.h"
#include "../../include/lq_canopen.h"
#include "../../include/lq_isotp.h"
#include "../../include/lq_uds.h"

/* Diagnostics */
#include "../../include/lq_dtc.h"

/* Arduino wrapper classes (optional) */
#ifdef ARDUINO
#include "LayeredQueue_BLDC.h"
#endif

#endif /* LAYEREDQUEUE_H_ */
