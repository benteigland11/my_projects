cmake_minimum_required(VERSION 3.12)

# *** Set Platform and Board variables FIRST ***
# Set these before including any SDK files or even the project() command.
set(PICO_PLATFORM rp2350 CACHE STRING "Pico Platform override")
set(PICO_BOARD pico2 CACHE STRING "Target board override")

# Include the SDK initialization cmake file FIRST.
# This defines the project() macro and other necessary things based on the SDK.
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

# Now define the project
project(my_project C CXX ASM)

# Initialize the Pico SDK (this uses the variables set earlier)
# It reads PICO_PLATFORM and PICO_BOARD set above.
pico_sdk_init()

# --- Rest of your CMakeLists.txt ---

add_executable(my_project
    main.cpp
    StepperMotor.cpp
    SerialMenu.cpp
    sd_card_manager.cpp
    openrocket_parser.cpp
    servo_controller.cpp
)

# Include directories for your project source files
target_include_directories(my_project PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

# Enable stdio output over USB VCP (Virtual COM Port)
pico_enable_stdio_usb(my_project 1)
# Disable stdio output over UART (can usually only use one at a time easily)
pico_enable_stdio_uart(my_project 0)

# Generate additional output formats like UF2
pico_add_extra_outputs(my_project)

# Link required SDK libraries
target_link_libraries(my_project PRIVATE
    pico_stdlib
    hardware_timer
    hardware_spi
    pico_fatfs
    hardware_flash
    hardware_pwm
    # Add other necessary hardware libraries here, e.g. hardware_i2c
)

if (NOT TARGET pico_fatfs)
    add_library(pico_fatfs INTERFACE)

    target_sources(pico_fatfs INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/fatfs/ff.c
        ${CMAKE_CURRENT_LIST_DIR}/fatfs/ffsystem.c
        ${CMAKE_CURRENT_LIST_DIR}/fatfs/ffunicode.c
        ${CMAKE_CURRENT_LIST_DIR}/tf_card.c
    )

    target_include_directories(pico_fatfs INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/fatfs
        ${CMAKE_CURRENT_LIST_DIR}/fatfs/conf
    )

    target_link_libraries(pico_fatfs INTERFACE
        pico_stdlib
        hardware_clocks
        hardware_spi
    )
endif()
