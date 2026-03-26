# template_plugin/CMakeLists.txt

# extract the plugin name（the last leg of the path）
get_filename_component(PLUGIN_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
string(REGEX REPLACE "_plugin$" "" PLUGIN_NAME ${PLUGIN_NAME})
# output path build_out/netclient_plugin
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PLUGIN_OUTPUT_DIR})

# plug in source files
file(GLOB PLUGIN_SOURCES *.cpp)

# compile to a shared library
add_library(${PLUGIN_NAME}_lib SHARED ${PLUGIN_SOURCES})
set_target_properties(${PLUGIN_NAME}_lib PROPERTIES OUTPUT_NAME ${PLUGIN_NAME})
set_target_properties(${PLUGIN_NAME}_lib PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
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
	ax_ivps_
	axcl_ivps_
	opencv_world
)

# define the plug in name macro for plugin_infer.cpp use
target_compile_definitions(${PLUGIN_NAME}_lib PRIVATE PLUGIN_NODE_NAME="${PLUGIN_NAME}")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/models")
    set(PLUGIN_RESOURCE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/models")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(PACK_TOOL "${CMAKE_SOURCE_DIR}/../thirdpark/tools/pack-linux-arm64")
else()
    set(PACK_TOOL "${CMAKE_SOURCE_DIR}/../thirdpark/tools/pack-linux-amd64")
endif()

set(PLUGIN_STAGING_DIR "${CMAKE_SOURCE_DIR}/build_out/.${PLUGIN_NAME}")
set(PLUGIN_PACKAGE_OUTPUT "${CMAKE_SOURCE_DIR}/build_out/${PLUGIN_NAME}.plugin")

add_custom_command(TARGET ${PLUGIN_NAME}_lib POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
            -DPLUGIN_NAME=${PLUGIN_NAME}
            -DPLUGIN_LIBRARY=$<TARGET_FILE:${PLUGIN_NAME}_lib>
            -DPACK_TOOL=${PACK_TOOL}
            -DPACKAGE_OUTPUT=${PLUGIN_PACKAGE_OUTPUT}
            -DSTAGING_DIR=${PLUGIN_STAGING_DIR}
            -DPLUGIN_RESOURCE_DIRS=${PLUGIN_RESOURCE_DIRS}
            -DGENERATE_CONFIG_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/../generate_config.cmake
            -P "${CMAKE_CURRENT_SOURCE_DIR}/../package_plugin.cmake"
    VERBATIM
)


# automatic check generation config.json whether it contains key information（simple protection）
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/config_template.json.in TEMPLATE_JSON)
string(FIND "${TEMPLATE_JSON}" "@PLUGIN_NAME@" NAME_PLACEHOLDER_FOUND)
if(NAME_PLACEHOLDER_FOUND EQUAL -1)
    message(FATAL_ERROR "config_template.json.in must include @PLUGIN_NAME@ placeholder.")
endif()

# Clean-up (optional)

add_custom_target(view_config
    COMMAND ${CMAKE_COMMAND} -E echo "📖 config.json:"
    COMMAND tar -xOzf ${CMAKE_SOURCE_DIR}/build_out/${PLUGIN_NAME}.plugin config.json | jq
)