set(CMAKE_CXX_STANDARD 20)

add_executable(jdk_npu_sample
	yoloFace.cpp
	main.cpp
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(jdk_npu_sample PRIVATE -Wno-psabi)
endif()

target_include_directories(jdk_npu_sample PRIVATE
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/thirdpark/jdk/include
	${CMAKE_SOURCE_DIR}/thirdpark/comm/include
	${CMAKE_SOURCE_DIR}/thirdpark/650nV2.25.1/include
	${CMAKE_SOURCE_DIR}/thirdpark/opencv4/include/opencv4
	${PROJECT_SOURCE_DIR}/netclient_interface
	${CMAKE_SOURCE_DIR}/thirdpark/backend_plugin/include
)

# target_compile_definitions(jdk_demo PRIVATE EXPORT_VISIBILITY="__attribute__((visibility(\"default\")))")
target_link_libraries(jdk_npu_sample PRIVATE
	core_sdk
	ax_frame
	axcl_ivps_
	opencv_world
	BackendHelper
	dl
)