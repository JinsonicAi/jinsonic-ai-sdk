cmake_minimum_required(VERSION 3.10)
project(jdk_sdk_demo)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -fPIC  -Os -s -w -lpthread -lrt -ldl ") # 
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -fPIC  -fsanitize=address -fno-omit-frame-pointer -g -lpthread -lrt -ldl ")
set(CMAKE_C_COMPILER "aarch64-none-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "aarch64-none-linux-gnu-g++")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-shlib-undefined")

include_directories(include
	thirdpark/jdk/include/
	thirdpark/comm/include
	thirdpark/650nv3.6.2/aibox/include 
	thirdpark/650nv3.6.2/axcl/axcl_linux_arm64/include  
	thirdpark/650nv3.6.2/axcl/axcl_linux_arm64/include/ffmpeg
	thirdpark/opencv4/include/opencv4
	thirdpark/openssl/include
	thirdpark/libfmt/include
)

link_directories(thirdpark/comm/lib
	thirdpark/opencv4/lib
	thirdpark/jdk/lib
	thirdpark/650nv3.6.2/aibox/lib
	thirdpark/650nv3.6.2/axcl/axcl_linux_arm64/lib
	thirdpark/650nv3.6.2/axcl/axcl_linux_arm64/lib/ffmpeg
	thirdpark/backend_plugin/lib
)

add_compile_options(-DEXPORT_VISIBILITY=__attribute__\(\(visibility\(\"default\"\)\)\))

# public library for samples
set(JDK_COMMON_LIBS
    core_sdk
    dl
    ax_sys
    axcl_rt
    ax_ivps
    ax_engine
    ax_node
    ax_pipeline
)

# declares core sdk as a shared library that has been built
add_library(core_sdk SHARED IMPORTED)
set_target_properties(core_sdk PROPERTIES
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/thirdpark/comm/lib/libcore_sdk.so"
)


# add_subdirectory(tools)
# add_subdirectory(netclient_interface)
add_subdirectory(example/jdk_node_sample)
add_subdirectory(example/jdk_frame_sample)
add_subdirectory(example/jdk_capture_sample)
add_subdirectory(example/jdk_ivps_sample)
add_subdirectory(example/jdk_netclient_vdec_sample)
add_subdirectory(example/jdk_netserver_venc_sample)
add_subdirectory(example/jdk_npu_sample)
