# notify mpolib that a config file exists
add_compile_definitions(HAVE_MPOLIB_CONFIG_H)

INCLUDE ( CheckIncludeFile )
CHECK_INCLUDE_FILE (sys/time.h HAVE_SYS_TIME_H)
CHECK_INCLUDE_FILE (unistd.h HAVE_UNISTD_H)
configure_file ( ${CMAKE_SOURCE_DIR}/src/mpolib_config_cmake.h ${CMAKE_BINARY_DIR}/mpolib_config.h )

# source files to be built
set(MPOLIB_SRCS
		mpo_client.cpp
        mpo_container.cpp
        mpo_file_finder.cpp
        mpo_file_stream.cpp
        mpo_fileio.cpp
        mpo_http.cpp
        mpo_httpd.cpp
        mpo_ipc.cpp
        mpo_memory_stream.cpp
        mpo_misc.cpp
        mpo_nat_stream.cpp
        mpo_net.cpp
        mpo_net_stream.cpp
        mpo_numstr.cpp
        mpo_process.cpp
        mpo_server.cpp
        mpo_socket_presenter.cpp
        mpo_stream.cpp
        mpo_thread.cpp
        mpo_timer.cpp
        mpo_udp.cpp
		md5c.cpp
        )

# headers to be installed
set(MPOLIB_PUBLIC_INCLUDE
	${CMAKE_SOURCE_DIR}/include/mpolib2/base64.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_client.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_container.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_deleter.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_dll.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_file_finder.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_fileio.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_file_stream.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_httpd.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_http.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_ipc.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_memory_stream.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_misc.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_nat_stream.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_net.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_net_stream.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_numstr.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_process.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_server.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_socket_presenter.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_ssl_stream.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_stream.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_thread.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_timer.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_types.h
	${CMAKE_SOURCE_DIR}/include/mpolib2/mpo_udp.h
	)

add_library(mpo2static ${MPOLIB_SRCS})

if (MSVC)
	# see https://devblogs.microsoft.com/cppblog/msvc-now-correctly-reports-__cplusplus/
	target_compile_options(mpo2static PUBLIC "/Zc:__cplusplus")
endif()

# So that anything that links to our lib gets the headers for all dependencies
# (configured include files go to the build folder, not the source folder (by design))
target_include_directories(mpo2static PUBLIC
        "${CMAKE_SOURCE_DIR}/include")

# we want libmpo2.a, not libmpo2static.a
set_target_properties(mpo2static PROPERTIES OUTPUT_NAME "mpo2")

# headers to be installed
set_target_properties(mpo2static PROPERTIES PUBLIC_HEADER "${MPOLIB_PUBLIC_INCLUDE}")

install(TARGETS mpo2static
	ARCHIVE DESTINATION lib
	PUBLIC_HEADER DESTINATION include/mpolib2
	)
