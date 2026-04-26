cmake_minimum_required(VERSION 3.10)
project(user_plugin)

set(CMAKE_CXX_STANDARD 20)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -fPIC  -Os -s -w -lpthread -lrt -ldl ") #
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -fPIC  -g -w -lpthread -lrt -ldl ")

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -fPIC  -fsanitize=address -fno-omit-frame-pointer -g -lpthread -lrt -ldl ")
set(CMAKE_C_COMPILER "aarch64-none-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "aarch64-none-linux-gnu-g++")

# plugin output directory
set(PLUGIN_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build_out)

# the path to the common header file required for plugin compilation（relative paths are adjusted according to your structure）
set(PLUGIN_COMMON_INCLUDE
	${CMAKE_SOURCE_DIR}/../include
	${CMAKE_SOURCE_DIR}/../thirdpark/jdk/include
	${CMAKE_SOURCE_DIR}/../thirdpark/comm/include
	${CMAKE_SOURCE_DIR}/../thirdpark/650nv3.10.2/aibox/include
	${CMAKE_SOURCE_DIR}/../thirdpark/650nv3.10.2/axcl/axcl_linux_arm64/include
	${CMAKE_SOURCE_DIR}/../thirdpark/650nv3.10.2/axcl/axcl_linux_arm64/include/ffmpeg
	${CMAKE_SOURCE_DIR}/../thirdpark/backend_plugin/include/
	${CMAKE_SOURCE_DIR}/../thirdpark/opencv4/include/opencv4
	${CMAKE_SOURCE_DIR}/../thirdpark/comm/include/gb28181
	${CMAKE_SOURCE_DIR}/../thirdpark/mqtt/include
	${CMAKE_SOURCE_DIR}/../thirdpark/openssl/include
	${CMAKE_SOURCE_DIR}/../thirdpark/libfmt/include
	${CMAKE_SOURCE_DIR}/../thirdpark/sqlite/include
	${CMAKE_SOURCE_DIR}/../thirdpark/live/include/liveMedia
	${CMAKE_SOURCE_DIR}/../thirdpark/live/include/BasicUsageEnvironment/
	${CMAKE_SOURCE_DIR}/../thirdpark/live/include/groupsock/
	${CMAKE_SOURCE_DIR}/../thirdpark/live/include/UsageEnvironment/
)

# public library directories
set(PLUGIN_COMMON_LINK_DIRS
	${CMAKE_SOURCE_DIR}/../thirdpark/jdk/lib
	${CMAKE_SOURCE_DIR}/../thirdpark/comm/lib
	${CMAKE_SOURCE_DIR}/../thirdpark/opencv4/lib
	${CMAKE_SOURCE_DIR}/../thirdpark/comm/lib/gb28181
	${CMAKE_SOURCE_DIR}/../thirdpark/mqtt/lib
	${CMAKE_SOURCE_DIR}/../thirdpark/libfmt/lib
	${CMAKE_SOURCE_DIR}/../thirdpark/sqlite/lib
)

set(PLUGIN_COMMON_LIBS
	pthread
	exif
	ax_frame
	loongrtcsdk
	ax_node
	ax_pipeline

	# JdkAxTaskManager
	opencv_world
	paho-mqttpp3
	paho-mqtt3a
)

# add_compile_definitions(EXPORT_VISIBILITY="__attribute__((visibility(\"default\")))")
add_compile_options(-DEXPORT_VISIBILITY=__attribute__\(\(visibility\(\"default\"\)\)\))

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	add_compile_options(-Wno-psabi)
endif()

# contains common headers
include_directories(${PLUGIN_COMMON_INCLUDE})
link_directories(${PLUGIN_COMMON_LINK_DIRS})

# find all plugin subdirectories（xxx_plugin）
file(GLOB PLUGIN_DIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*_plugin)

set(PLUGIN_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build_out)
file(MAKE_DIRECTORY ${PLUGIN_OUTPUT_PATH})

foreach(PLUGIN_NAME ${PLUGIN_DIRS})
	if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${PLUGIN_NAME})
		# add subprojects dynamically
		message(STATUS "Configuring plugin: ${PLUGIN_NAME}")
		add_subdirectory(${PLUGIN_NAME})
	endif()
endforeach()
