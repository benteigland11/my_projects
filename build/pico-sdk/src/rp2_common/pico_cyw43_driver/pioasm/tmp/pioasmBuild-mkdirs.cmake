# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/benteigland/pico/pico-sdk/tools/pioasm"
  "/home/benteigland/pico/my_projects/build/pioasm"
  "/home/benteigland/pico/my_projects/build/pioasm-install"
  "/home/benteigland/pico/my_projects/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/tmp"
  "/home/benteigland/pico/my_projects/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp"
  "/home/benteigland/pico/my_projects/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src"
  "/home/benteigland/pico/my_projects/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/benteigland/pico/my_projects/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/benteigland/pico/my_projects/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp${cfgdir}") # cfgdir has leading slash
endif()
