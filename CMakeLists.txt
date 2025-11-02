# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)
set(PROJECT_VER "1.0.0")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(lightbox)

#add_custom_command(
#        TARGET lightbox.elf
#        COMMAND ${CMAKE_COMMAND} -E copy_directory
#            ${CMAKE_CURRENT_SOURCE_DIR}/lightbox-ui/dist
#            ${CMAKE_CURRENT_BINARY_DIR}/web
#        PRE_BUILD
#)


target_add_binary_data(lightbox.elf "lightbox-ui/dist/assets/index.css.gz" BINARY RENAME_TO "assets/index.css")
target_add_binary_data(lightbox.elf "lightbox-ui/dist/assets/index.js.gz" BINARY RENAME_TO "assets/index.js")
target_add_binary_data(lightbox.elf "lightbox-ui/dist/index.html.gz" BINARY RENAME_TO "index.html")
target_add_binary_data(lightbox.elf "lightbox-ui/dist/favicon.svg.gz" BINARY RENAME_TO "favicon.svg")
