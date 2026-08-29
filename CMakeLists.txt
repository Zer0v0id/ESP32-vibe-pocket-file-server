# Vibe Pocket File Server - ESP32-S3
cmake_minimum_required(VERSION 3.16)

# Two firmware variants:
#   idf.py build                              -> no OLED (default)
#   idf.py -B build-display -DWITH_DISPLAY=ON -> SSD1306 OLED
option(WITH_DISPLAY "Build with SSD1306 OLED status display" OFF)

if(WITH_DISPLAY)
    set(SDKCONFIG "${CMAKE_SOURCE_DIR}/sdkconfig.display")
    set(SDKCONFIG_DEFAULTS "${CMAKE_SOURCE_DIR}/sdkconfig.defaults;${CMAKE_SOURCE_DIR}/sdkconfig.defaults.display")
else()
    set(SDKCONFIG "${CMAKE_SOURCE_DIR}/sdkconfig")
    set(SDKCONFIG_DEFAULTS "${CMAKE_SOURCE_DIR}/sdkconfig.defaults")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(vibe_pocket_file_server)
