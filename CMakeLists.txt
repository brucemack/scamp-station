cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(hello-lcd C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# PICO build
pico_sdk_init()

add_executable(main
  main.cpp 
  main2.cpp 
  StationDemodulatorListener.cpp
  Si5351FSKModulator.cpp
  EditorState.cpp
  radlib/util/WindowAverage.cpp
  radlib/util/dsp_util.cpp
  radlib/util/fixed_math.cpp
  radlib/util/fixed_fft.cpp
  radlib/util/Demodulator.cpp 
  radlib/lcd/HD44780.cpp 
  radlib/lcd/HD44780_PCF8574.cpp 
  radlib/hw/pico/PICOI2CInterface.cpp 
  radlib/hw/pico/PICOClockInterface.cpp
  radlib/rtty/BaudotEncoder.cpp
  radlib/rtty/BaudotDecoder.cpp
  radlib/rtty/RTTYDemodulator.cpp
  radlib/morse/MorseEncoder.cpp
  radlib/tests/TestI2CInterface.cpp 
  radlib/tests/TestClockInterface.cpp
  radlib/scamp/Symbol6.cpp 
  radlib/scamp/CodeWord12.cpp  
  radlib/scamp/CodeWord24.cpp 
	radlib/scamp/Frame30.cpp 
  radlib/scamp/Util.cpp 
	radlib/scamp/ClockRecoveryPLL.cpp 
	radlib/scamp/ClockRecoveryDLL.cpp 
  radlib/scamp/SCAMPDemodulator.cpp 
  hello-ps2keyboard/PS2Keyboard.cpp
  hello-pico-si5351/si5351.c 
	)
pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 1)
target_compile_definitions(main PUBLIC PICO_BUILD=1)
target_link_libraries(main pico_stdlib hardware_i2c hardware_adc pico_multicore)

