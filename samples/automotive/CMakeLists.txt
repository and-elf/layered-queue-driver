# Automotive control system example
# Demonstrates multi-source sensor fusion with voting and merge logic

add_lq_application(automotive_app
    DTS app.dts
    PLATFORM native
    RTOS baremetal
    ENABLE_HIL_TESTS
)
