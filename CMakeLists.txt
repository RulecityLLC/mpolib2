cmake_minimum_required(VERSION 3.15)
project(mpo2)

# define library version
set(MPOLIB_VERSION_MAJOR 2 CACHE STRING "major version" FORCE)
set(MPOLIB_VERSION_MINOR 0 CACHE STRING "minor version" FORCE)
set(MPOLIB_VERSION ${MPOLIB_VERSION_MAJOR}.${MPOLIB_VERSION_MINOR} CACHE STRING "version" FORCE)

# from https://foonathan.net/2016/03/cmake-install/
set(include_dest "include/mpo2-${MPOLIB_VERSION}")
set(main_lib_dest "lib/mpo2-${MPOLIB_VERSION}")

# for config.h file since it may be different depending on whether we are doing Debug/Release/etc
set(lib_dest "${main_lib_dest}/${CMAKE_BUILD_TYPE}")

# don't look back
set(CMAKE_CXX_STANDARD 17)

# TODO : move test stuff into test CMake?

# so that gmock doesn't get a different runtime than the rest of the system when using Visual Studio
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(INSTALL_GTEST "Enable installation of googletest. (Projects embedding googletest may want to turn this OFF.)" OFF)
add_subdirectory("thirdparty/googletest-release-1.8.1")

add_subdirectory("src")
add_subdirectory("tests")

configure_file(mpo2-config-version.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/mpo2-config-version.cmake @ONLY)

install(FILES mpo2-config.cmake ${CMAKE_CURRENT_BINARY_DIR}/mpo2-config-version.cmake DESTINATION ${main_lib_dest})
install(EXPORT mpo2 FILE mpo2.cmake DESTINATION "${lib_dest}")
