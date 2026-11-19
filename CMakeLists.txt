cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# # Initialise pico_sdk from installed location
# # (note this can come from environment, CMake cache etc)
# # == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
set(USERHOME $ENV{USERPROFILE})
else()
set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
include(${picoVscode})
endif()
#

set(PICO_BOARD pico CACHE STRING "Board type")
include(pico_sdk_import.cmake)
project(ultra_simon C CXX ASM)
pico_sdk_init()
add_executable(ultra_simon
ultra_simon.c
)
pico_set_program_name(ultra_simon "ultra_simon")
pico_set_program_version(ultra_simon "0.1")
add_subdirectory(pico-st7735/lib-st7735)
target_link_libraries(ultra_simon
pico_stdlib
hardware_spi
st7735
hardware_adc
hardware_timer
hardware_pwm
)
target_include_directories(ultra_simon PRIVATE
${CMAKE_CURRENT_LIST_DIR}
${CMAKE_CURRENT_LIST_DIR}/pico-st7735/lib-st7735/include
)
target_compile_definitions(ultra_simon PRIVATE
SPI_PORT=spi1
SPI_TX=11
SPI_SCK=10
SPI_TFT_PORT=spi1
SPI_TFT_CS=9
SPI_TFT_RST=12
SPI_TFT_DC=13
TFT_ENABLE_BLACK
TFT_ENABLE_RESET
TFT_ENABLE_TEXT
TFT_ENABLE_SHAPES
)
target_compile_definitions(st7735 PRIVATE
SPI_PORT=spi1
SPI_TX=11
SPI_SCK=10
SPI_TFT_PORT=spi1
SPI_TFT_CS=9
SPI_TFT_RST=12
SPI_TFT_DC=13
TFT_ENABLE_BLACK
TFT_ENABLE_RESET
TFT_ENABLE_TEXT
TFT_ENABLE_SHAPES
)
pico_add_extra_outputs(ultra_simon)