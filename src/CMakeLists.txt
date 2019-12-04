# notify mpolib that a config file exists
add_compile_definitions(HAVE_MPOLIB_CONFIG_H)

INCLUDE ( CheckIncludeFile )
CHECK_INCLUDE_FILE (sys/time.h HAVE_SYS_TIME_H)
CHECK_INCLUDE_FILE (unistd.h HAVE_UNISTD_H)
configure_file ( ${CMAKE_SOURCE_DIR}/src/mpolib_config_cmake.h ${CMAKE_BINARY_DIR}/mpolib_config.h )

# for convenience
set(header_path "${CMAKE_SOURCE_DIR}/include/mpolib2")

# headers to be installed
set(MPOLIB_PUBLIC_INCLUDE
		${header_path}/mpo_client.h
		${header_path}/mpo_container.h
		${header_path}/mpo_deleter.h
		${header_path}/mpo_dll.h
		${header_path}/mpo_file_finder.h
		${header_path}/mpo_fileio.h
		${header_path}/mpo_file_stream.h
		${header_path}/mpo_httpd.h
		${header_path}/mpo_http.h
		${header_path}/mpo_ipc.h
		${header_path}/mpo_memory_stream.h
		${header_path}/mpo_misc.h
		${header_path}/mpo_nat_stream.h
		${header_path}/mpo_net.h
		${header_path}/mpo_net_stream.h
		${header_path}/mpo_numstr.h
		${header_path}/mpo_process.h
		${header_path}/mpo_server.h
		${header_path}/mpo_socket_presenter.h
		${header_path}/mpo_stream.h
		${header_path}/mpo_thread.h
		${header_path}/mpo_timer.h
		${header_path}/mpo_types.h
		${header_path}/mpo_udp.h
		)

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

add_library(mpo2 ${MPOLIB_PUBLIC_INCLUDE} ${MPOLIB_SRCS} )

if (MSVC)
	# see https://devblogs.microsoft.com/cppblog/msvc-now-correctly-reports-__cplusplus/
	target_compile_options(mpo2 PUBLIC "/Zc:__cplusplus")
endif()

# So that anything that links to our lib gets the headers for all dependencies
# (configured include files go to the build folder, not the source folder (by design))
target_include_directories(mpo2 PUBLIC

		# this will be populated when installing but blank when building
		# for client in install mode
		$<INSTALL_INTERFACE:${include_dest}>
		$<INSTALL_INTERFACE:${lib_dest}> # for config.h file

		# this will be populated when building but blank when installing
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>)


# we want libmpo2.a, not libmpo2static.a
#set_target_properties(mpo2 PROPERTIES OUTPUT_NAME "mpo2")

# headers to be installed
# set_target_properties(mpo2 PROPERTIES PUBLIC_HEADER "${MPOLIB_PUBLIC_INCLUDE}")

#install(TARGETS mpo2
#	ARCHIVE DESTINATION lib
#	PUBLIC_HEADER DESTINATION include/mpolib2
#	)

install(FILES ${MPOLIB_PUBLIC_INCLUDE} DESTINATION "${include_dest}/mpolib")

# goes into lib_dest so that Debug/Release is taken into account
install(TARGETS mpo2 EXPORT mpo2 DESTINATION "${lib_dest}")
install(FILES ${CMAKE_BINARY_DIR}/mpolib_config.h DESTINATION "${lib_dest}")