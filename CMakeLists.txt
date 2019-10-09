cmake_minimum_required(VERSION 3.15)
project(mpo2)

# don't look back
set(CMAKE_CXX_STANDARD 17)

# so that gmock doesn't get a different runtime than the rest of the system when using Visual Studio
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(INSTALL_GTEST "Enable installation of googletest. (Projects embedding googletest may want to turn this OFF.)" OFF)
add_subdirectory("thirdparty/googletest-release-1.8.1")

add_subdirectory("src")
add_subdirectory("tests")
