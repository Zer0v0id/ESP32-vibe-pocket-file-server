# Vibe Pocket File Server - ESP32-S3
cmake_minimum_required(VERSION 3.16)

# Generic DevKit: SSD1306 OLED included; turn the screen off in Settings.
# LilyGO T-Embed CC1101: idf.py -DBOARD=t-embed-cc1101 build
if(NOT DEFINED BOARD AND DEFINED ENV{BOARD})
    set(BOARD "$ENV{BOARD}")
endif()

if(BOARD STREQUAL "t-embed-cc1101")
    message(STATUS "Board: LilyGO T-Embed CC1101")
    set(SDKCONFIG "${CMAKE_SOURCE_DIR}/sdkconfig.t-embed")
    set(SDKCONFIG_DEFAULTS
        "${CMAKE_SOURCE_DIR}/sdkconfig.defaults"
        "${CMAKE_SOURCE_DIR}/sdkconfig.defaults.t-embed-cc1101")
else()
    set(SDKCONFIG "${CMAKE_SOURCE_DIR}/sdkconfig")
    set(SDKCONFIG_DEFAULTS "${CMAKE_SOURCE_DIR}/sdkconfig.defaults")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(vibe_pocket_file_server)
