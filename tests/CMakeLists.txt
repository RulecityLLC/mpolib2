# this config file should exist from when we built the source and may be needed to avoided weird build issues
add_compile_definitions(HAVE_MPOLIB_CONFIG_H)

set(TEST_MPOLIB_SRCS
        big_endian_test.cpp
        binhex_test.cpp
        main.cpp
        test_container.cpp
        test_file_find.cpp
        test_http.cpp
        test_httpd.cpp
        test_nat_stream.cpp
        test_numstr.cpp
        test_pipe.cpp
        test_process.cpp
        test_server.cpp
        test_stream.cpp
        test_thread.cpp
        test_throw.cpp
        test_udp.cpp
        test_utf8.cpp
        )

# we want all our little test programs to be in the same dir as the test exe
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# MSVC by default appends 'debug' and 'release' to its runtime output folder.
# This makes our custom build step (copying data needed by tests) difficult to work with.
# So we just remove those suffixes and have a single runtime output folder for MSVC.
# It is a little messy, but it's the best solution I've been able to come up with for now.
if (MSVC)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()

add_executable(test_mpolib ${TEST_MPOLIB_SRCS})
add_subdirectory("std_test_exiter_src")
add_subdirectory("std_test_helper_src")
add_subdirectory("std_test_wait_for_stdin_src")
add_subdirectory("test_exit_after_10_src")
add_subdirectory("test_wide_helper_src")

include_directories("../thirdparty/googletest-release-1.8.1/googlemock/include")
include_directories("../thirdparty/googletest-release-1.8.1/googletest/include")

# the tests rely on these helper programs being built
add_dependencies(test_mpolib std_test_exiter)
add_dependencies(test_mpolib std_test_helper)
add_dependencies(test_mpolib std_test_wait_for_stdin)
add_dependencies(test_mpolib test_exit_after_10)
add_dependencies(test_mpolib test_wide_helper)

# add a post-build step to copy resources needed for tests
add_custom_command(
    TARGET test_mpolib POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_SOURCE_DIR}/tests/test_mpolib_dummy.zip
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mpolib_dummy.zip)

# make visual studio run from the correct directory (only works for VS2010 and above)
set_property(TARGET test_mpolib PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set_property(TARGET test_mpolib PROPERTY VS_DEBUGGER_COMMAND_ARGUMENTS "-quick")

# this will automatically give the indicated targets access to the headers/libs of the indicated dependencies
if(WIN32)
    if (MSVC)
        # see https://devblogs.microsoft.com/cppblog/msvc-now-correctly-reports-__cplusplus/
        target_compile_options(test_mpolib PUBLIC "/Zc:__cplusplus")
        target_link_libraries(test_mpolib LINK_PUBLIC mpo2static gmock ws2_32)
        target_link_libraries(test_wide_helper LINK_PUBLIC mpo2static)
    else()
        # we don't want to have to fiddle around with the GCC windows DLLs, so we have to manually append "-static" to the end
        target_link_libraries(test_mpolib LINK_PUBLIC mpo2static gmock ws2_32 -static)
        target_link_libraries(test_wide_helper LINK_PUBLIC mpo2static -static)
        target_link_libraries(std_test_exiter -static)
        target_link_libraries(std_test_helper -static)
        target_link_libraries(std_test_wait_for_stdin -static)
        target_link_libraries(test_exit_after_10 -static)
    endif()
else()
    # we assume in most unix-ish environments that the compiler's runtime is in the PATH (no need to statically link test programs)
    target_link_libraries(test_mpolib LINK_PUBLIC mpo2static gmock)
    target_link_libraries(test_wide_helper LINK_PUBLIC mpo2static)
endif()
