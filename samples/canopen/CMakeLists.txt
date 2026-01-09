# CANopen motor driver example  
# Demonstrates build-time code generation from DTS with EDS expansion

# Use the LayeredQueue application generator
add_lq_application(canopen_motor_driver
    DTS motor_system.dts
    EDS example_motor.eds
    PLATFORM native
    RTOS baremetal
)

# Post-build messages
add_custom_command(TARGET canopen_motor_driver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    COMMAND ${CMAKE_COMMAND} -E echo "Generated CANopen BLDC Motor Driver:"
    COMMAND ${CMAKE_COMMAND} -E echo "  Input:  motor_system.dts"
    COMMAND ${CMAKE_COMMAND} -E echo "  Input:  example_motor.eds"
    COMMAND ${CMAKE_COMMAND} -E echo "  Output: lq_generated.c/h"
    COMMAND ${CMAKE_COMMAND} -E echo "  Output: motor_signals.h"
    COMMAND ${CMAKE_COMMAND} -E echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    COMMAND ${CMAKE_COMMAND} -E echo ""
)
