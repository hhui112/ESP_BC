# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# 与 common.h 中 INIT_VERSION 一致，写入 esp_app_desc.version（回退读分区版本、与 NVS 业务号对齐）
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/components/common/common.h" _common_h_content)
string(REGEX MATCH "#define[ \t]+INIT_VERSION[ \t]+\"([^\"]+)\"" _ "${_common_h_content}")
if(NOT CMAKE_MATCH_1)
    message(FATAL_ERROR "INIT_VERSION not found in components/common/common.h")
endif()
set(PROJECT_VER "${CMAKE_MATCH_1}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# set(CMAKE_EXE_LINKER_FLAGS "-Wl,-Map=output.map")
project(bochuang-program)