set(CMAKE_CXX_STANDARD 20)

add_executable(jdk_netserver_sample
jdk_netserver_sample.cpp
)

target_include_directories(jdk_netserver_sample PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/thirdpark/jdk/include
    ${CMAKE_SOURCE_DIR}/thirdpark/comm/include
    ${CMAKE_SOURCE_DIR}/thirdpark/650nV2.25.1/include
    ${CMAKE_SOURCE_DIR}/thirdpark/opencv4/include/opencv4
	${PROJECT_SOURCE_DIR}/netclient_interface
)
# target_compile_definitions(jdk_demo PRIVATE EXPORT_VISIBILITY="__attribute__((visibility(\"default\")))")

target_link_libraries(jdk_netserver_sample PRIVATE
	core_sdk
	ax_frame
	axcl_venc_
	dl
)