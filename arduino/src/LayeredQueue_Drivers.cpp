/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Build all driver source files for Arduino
 * 
 * This file includes all .c files from the parent repository.
 * The Arduino build system will compile this single file which
 * in turn includes all the driver code.
 * 
 * NOTE: We need to define LQ_INCLUDE_PATH so the .c files can find headers
 */

#ifdef ARDUINO

/* Arduino AVR doesn't support C11 _Static_assert, so stub it out */
#ifndef _Static_assert
#define _Static_assert(cond, msg) /* AVR compatibility */
#endif

/* Tell the .c files where to find headers (relative to this file) */
#define LQ_INCLUDE_PREFIX "../../include/"

/* Include all driver source files from parent repository */
#include "../../src/drivers/lq_util.c"
#include "../../src/drivers/lq_queue_core.c"
#include "../../src/drivers/lq_hw_input.c"
#include "../../src/drivers/lq_engine.c"
#include "../../src/drivers/lq_scale.c"
#include "../../src/drivers/lq_remap.c"
#include "../../src/drivers/lq_pid.c"
#include "../../src/drivers/lq_verified_output.c"
#include "../../src/drivers/lq_bldc.c"
#include "../../src/drivers/lq_j1939.c"
#include "../../src/drivers/lq_canopen.c"
#include "../../src/drivers/lq_isotp.c"
#include "../../src/drivers/lq_uds.c"
#include "../../src/drivers/lq_uds_can.c"
#include "../../src/drivers/lq_dtc.c"
#include "../../src/drivers/lq_spi_source.c"
#include "../../src/drivers/lq_config.c"

/* HIL and platform-specific drivers excluded from Arduino (need sockets/pthread) */
/* #include "../../src/drivers/lq_hil.c" */
/* #include "../../src/drivers/lq_hil_platform.c" */

#endif /* ARDUINO */
