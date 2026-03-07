# template_plugin/CMakeLists.txt

# extract the plugin name（the last leg of the path）
get_filename_component(PLUGIN_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
string(REGEX REPLACE "_plugin$" "" PLUGIN_NAME ${PLUGIN_NAME})
set(PLUGIN_CLEANUP_COMMAND ${CMAKE_COMMAND} -E rm -rf)
# output path plugins_out/netclient_plugin
set(PLUGIN_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build_out/.${PLUGIN_NAME})
file(MAKE_DIRECTORY ${PLUGIN_OUTPUT_PATH})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PLUGIN_OUTPUT_DIR})

# plug in source files
file(GLOB PLUGIN_SOURCES *.cpp)

include_directories(
	../../thirdpark/curl/include/
)

link_directories(
	../../thirdpark/curl/lib
)

# compile to a shared library
add_library(${PLUGIN_NAME}_lib SHARED ${PLUGIN_SOURCES})
set_target_properties(${PLUGIN_NAME}_lib PROPERTIES OUTPUT_NAME ${PLUGIN_NAME})
set_target_properties(${PLUGIN_NAME}_lib PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${PLUGIN_OUTPUT_PATH})

#  insert sanitizer flags preferably immediately after add library
if(DEFINED ENV{ENABLE_ASAN})
    message(STATUS "ASan enabled")
	set(SANITIZER_FLAGS "-fsanitize=address" "-fsanitize=undefined" "-fno-omit-frame-pointer" "-g" "-fsanitize-address-use-after-scope")
	target_compile_options(${PLUGIN_NAME}_lib PRIVATE ${SANITIZER_FLAGS})
	target_link_options(${PLUGIN_NAME}_lib PRIVATE ${SANITIZER_FLAGS})
endif()

# links to public libraries defined by the parent directory
target_link_libraries(${PLUGIN_NAME}_lib PRIVATE 
${PLUGIN_COMMON_LIBS} 
	# NetClient
)

# define the plug in name macro for plugin_infer.cpp use
target_compile_definitions(${PLUGIN_NAME}_lib PRIVATE PLUGIN_NODE_NAME="${PLUGIN_NAME}")

# automatically generated config.json
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config_template.json.in
    ${PLUGIN_OUTPUT_PATH}/config.json
    @ONLY
)

option(PLUGIN_REMOVE_AFTER_PACKAGE "Whether to remove plugin source files after packaging" OFF)
set(PLUGIN_REMOVE_AFTER_PACKAGE ON)

add_custom_command(TARGET ${PLUGIN_NAME}_lib PRE_LINK
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_OUTPUT_PATH}
)

# after target link add
add_custom_command(TARGET ${PLUGIN_NAME}_lib POST_BUILD
  COMMAND ${CMAKE_COMMAND}
    -DPLUGIN_OUTPUT_PATH=${PLUGIN_OUTPUT_PATH}
    -DPLUGIN_NAME=${PLUGIN_NAME}
    -DPLUGIN_VERSION=${PLUGIN_VERSION}
    -DPLUGIN_ARCH=${PLUGIN_ARCH}
    -DPLUGIN_OS=${PLUGIN_OS}
    -DPLUGIN_CHIP=${PLUGIN_CHIP}
    -DPLUGIN_BUILD_TIME=${PLUGIN_BUILD_TIME}
    -P "${CMAKE_CURRENT_SOURCE_DIR}/../generate_config.cmake"
)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(PACK_TOOL "${CMAKE_SOURCE_DIR}/../thirdpark/tools/pack-linux-arm64")
else()
    set(PACK_TOOL "${CMAKE_SOURCE_DIR}/../thirdpark/tools/pack-linux-amd64")
endif()

add_custom_command(TARGET ${PLUGIN_NAME}_lib POST_BUILD
    COMMAND "${PACK_TOOL}" -C "${PLUGIN_OUTPUT_PATH}"
            -o "${CMAKE_SOURCE_DIR}/build_out/${PLUGIN_NAME}.plugin"
    if(PLUGIN_REMOVE_AFTER_PACKAGE)
        COMMAND ${PLUGIN_CLEANUP_COMMAND} "${PLUGIN_OUTPUT_PATH}"
    endif()
    VERBATIM
)

# automatic check generation config.json whether it contains key information（simple protection）
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/config_template.json.in TEMPLATE_JSON)
string(FIND "${TEMPLATE_JSON}" "@PLUGIN_NAME@" NAME_PLACEHOLDER_FOUND)
if(NAME_PLACEHOLDER_FOUND EQUAL -1)
    message(FATAL_ERROR "config_template.json.in must include @PLUGIN_NAME@ placeholder.")
endif()

# Clean-up (optional)
if(PLUGIN_REMOVE_AFTER_PACKAGE)
    add_custom_command(TARGET ${PLUGIN_NAME}_lib POST_BUILD
        COMMAND ${PLUGIN_CLEANUP_COMMAND} "${PLUGIN_OUTPUT_PATH}"
    )
endif()

# add_custom_target(view_config
#     COMMAND ${CMAKE_COMMAND} -E echo "📖 config.json:"
#     COMMAND tar -xOzf ${CMAKE_SOURCE_DIR}/plugins_out/${PLUGIN_NAME}.plugin config.json | jq
# )