# Vibe Pocket File Server - ESP32-S3
cmake_minimum_required(VERSION 3.16)

set(SDKCONFIG_DEFAULTS "${CMAKE_SOURCE_DIR}/sdkconfig.defaults")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(vibe_pocket_file_server)
