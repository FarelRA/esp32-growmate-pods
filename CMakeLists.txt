cmake_minimum_required(VERSION 3.16)

list(APPEND EXTRA_COMPONENT_DIRS components/esp-idf-lib/components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_cam_flash_blink)
