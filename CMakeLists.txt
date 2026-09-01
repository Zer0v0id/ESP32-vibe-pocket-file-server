# Vibe Pocket File Server - ESP32-S3
cmake_minimum_required(VERSION 3.16)

# One firmware: SSD1306 OLED driver is included. Turn the screen off in
# Settings if you have no display. If no OLED is wired, init fails quietly.
set(SDKCONFIG "${CMAKE_SOURCE_DIR}/sdkconfig")
set(SDKCONFIG_DEFAULTS "${CMAKE_SOURCE_DIR}/sdkconfig.defaults")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(vibe_pocket_file_server)
