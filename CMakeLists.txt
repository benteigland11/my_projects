cmake_minimum_required(VERSION 3.12)

set(PICO_SDK_PATH "C:/pico/pico-sdk")
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

project(my_project C CXX ASM)
set(PICO_BOARD "pico2" CACHE STRING "Board to build for")
pico_sdk_init()

add_executable(my_project
    main.cpp
    StepperMotor.cpp
    SerialMenu.cpp
    SDCardManager.cpp
    fatfs/ff.c
)

target_include_directories(my_project PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/fatfs
)

pico_enable_stdio_usb(my_project 1)
pico_enable_stdio_uart(my_project 0)
pico_add_extra_outputs(my_project)

target_link_libraries(my_project
    pico_stdlib
    hardware_timer
    hardware_spi
)