# Copyright (c) 2026 Layered Queue Driver
# SPDX-License-Identifier: Apache-2.0

# Add layered queue drivers
add_subdirectory_ifdef(CONFIG_LQ_DRIVER layered_queue)
