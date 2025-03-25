# Minimum CMake version required
cmake_minimum_required(VERSION 3.12)

# Set the Pico SDK path
set(PICO_SDK_PATH "C:/pico/pico-sdk")

# Include the Pico SDK
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

# Project name (supports C, C++, and ASM)
project(my_project C CXX ASM)

# Initialize the SDK
pico_sdk_init()

# Disable versioning to avoid linker error
set(PICO_NO_IMAGE_VERSIONING 1)  # Fixed typo here

# Add your source file(s)
add_executable(my_project
    main.cpp
)

# Enable USB output, disable UART
pico_enable_stdio_usb(my_project 1)
pico_enable_stdio_uart(my_project 0)

# Generate extra output files (like .uf2)
pico_add_extra_outputs(my_project)

# Link Pico SDK libraries
target_link_libraries(my_project
    pico_stdlib
)