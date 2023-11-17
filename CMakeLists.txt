cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(hello-lcd C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# PICO build
pico_sdk_init()

add_executable(main
  main.cpp hello-lcd/HD44780.cpp hello-lcd/HD44780_PCF8574.cpp 
  hello-lcd/tests/TestI2CInterface.cpp 
  hello-lcd/tests/TestClockInterface.cpp
  hello-lcd/pico/PICOI2CInterface.cpp 
  hello-lcd/pico/PICOClockInterface.cpp
)
pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 1)
target_compile_definitions(main PUBLIC PICO_BUILD=1)
target_link_libraries(main pico_stdlib hardware_i2c)



