# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/hoagson/pico/pico-sdk/tools/pioasm"
  "/mnt/c/SAXION/Year 2/Quarter 1/Microcontroller/week 610/project-template/build/pioasm"
  "/mnt/c/SAXION/Year 2/Quarter 1/Microcontroller/week 610/project-template/build/pioasm-install"
  "/mnt/c/SAXION/Year 2/Quarter 1/Microcontroller/week 610/project-template/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/tmp"
  "/mnt/c/SAXION/Year 2/Quarter 1/Microcontroller/week 610/project-template/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp"
  "/mnt/c/SAXION/Year 2/Quarter 1/Microcontroller/week 610/project-template/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src"
  "/mnt/c/SAXION/Year 2/Quarter 1/Microcontroller/week 610/project-template/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/mnt/c/SAXION/Year 2/Quarter 1/Microcontroller/week 610/project-template/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/mnt/c/SAXION/Year 2/Quarter 1/Microcontroller/week 610/project-template/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp${cfgdir}") # cfgdir has leading slash
endif()
