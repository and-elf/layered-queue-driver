# Copyright (c) 2026 Layered Queue Driver
# SPDX-License-Identifier: Apache-2.0

zephyr_library()

# Core library (platform-independent)
zephyr_library_sources(../../src/drivers/lq_queue_core.c)
zephyr_library_sources(../../src/drivers/lq_util.c)
zephyr_library_sources(../../src/drivers/lq_spi_source.c)
zephyr_library_sources(../../src/platform/lq_platform_zephyr.c)

# Protocol drivers (platform-independent)
zephyr_library_sources_ifdef(CONFIG_LQ_PROTOCOL_J1939 ../../src/drivers/lq_j1939.c)
zephyr_library_sources_ifdef(CONFIG_LQ_PROTOCOL_CANOPEN ../../src/drivers/lq_canopen.c)

# Zephyr device tree wrappers
zephyr_library_sources_ifdef(CONFIG_LQ_QUEUE lq_queue.c)
zephyr_library_sources_ifdef(CONFIG_LQ_ADC_SOURCE lq_adc_source.c)
zephyr_library_sources_ifdef(CONFIG_LQ_SPI_SOURCE lq_spi_source.c)
zephyr_library_sources_ifdef(CONFIG_LQ_MERGE_VOTER lq_merge_voter.c)
zephyr_library_sources_ifdef(CONFIG_LQ_DUAL_INVERTED lq_dual_inverted.c)
zephyr_library_sources_ifdef(CONFIG_LQ_PROTOCOL_J1939 lq_protocol_j1939.c)
zephyr_library_sources_ifdef(CONFIG_LQ_PROTOCOL_CANOPEN lq_protocol_canopen.c)

zephyr_library_sources(lq_util.c)

zephyr_include_directories(../../include)
