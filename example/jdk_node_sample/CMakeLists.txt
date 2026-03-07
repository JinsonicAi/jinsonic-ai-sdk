set(CMAKE_CXX_STANDARD 20)


add_executable(jdk_demo
    jdk_demo.cpp
)

target_include_directories(jdk_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/thirdpark/jdk/include
    ${CMAKE_SOURCE_DIR}/thirdpark/comm/include
    ${CMAKE_SOURCE_DIR}/thirdpark/650nV2.25.1/include
    ${CMAKE_SOURCE_DIR}/thirdpark/opencv4/include/opencv4
	${PROJECT_SOURCE_DIR}/netclient_interface
)
# target_compile_definitions(jdk_demo PRIVATE EXPORT_VISIBILITY="__attribute__((visibility(\"default\")))")

target_link_libraries(jdk_demo PRIVATE
	core_sdk
	dl
	${JDK_COMMON_LIBS}
)