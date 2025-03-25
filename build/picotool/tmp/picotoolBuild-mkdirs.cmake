# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/pico/my_projects/build/_deps/picotool-src"
  "C:/pico/my_projects/build/_deps/picotool-build"
  "C:/pico/my_projects/build/_deps"
  "C:/pico/my_projects/build/picotool/tmp"
  "C:/pico/my_projects/build/picotool/src/picotoolBuild-stamp"
  "C:/pico/my_projects/build/picotool/src"
  "C:/pico/my_projects/build/picotool/src/picotoolBuild-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/pico/my_projects/build/picotool/src/picotoolBuild-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/pico/my_projects/build/picotool/src/picotoolBuild-stamp${cfgdir}") # cfgdir has leading slash
endif()
