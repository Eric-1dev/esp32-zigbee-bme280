cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32h2-zigbee-sensor)
set(EXTRA_COMPONENT_DIRS ${PROJECT_DIR}/components/esp-zigbee-sdk)