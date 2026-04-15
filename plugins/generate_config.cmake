# generate_config.cmake

if(NOT DEFINED PLUGIN_OS OR PLUGIN_OS STREQUAL "")
  execute_process(COMMAND uname -s OUTPUT_VARIABLE PLUGIN_OS OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

if(NOT DEFINED PLUGIN_ARCH OR PLUGIN_ARCH STREQUAL "")
  if(DEFINED ENV{AIBOX_PLUGIN_ARCH} AND NOT "$ENV{AIBOX_PLUGIN_ARCH}" STREQUAL "")
    set(PLUGIN_ARCH "$ENV{AIBOX_PLUGIN_ARCH}")
  else()
    set(PLUGIN_ARCH "aarch64")
  endif()
endif()

if(NOT DEFINED PLUGIN_VERSION OR PLUGIN_VERSION STREQUAL "")
  if(DEFINED ENV{AIBOX_PLUGIN_VERSION} AND NOT "$ENV{AIBOX_PLUGIN_VERSION}" STREQUAL "")
    set(PLUGIN_VERSION "$ENV{AIBOX_PLUGIN_VERSION}")
  else()
    set(PLUGIN_VERSION "3.6.2")
  endif()
endif()

if(NOT DEFINED PLUGIN_CHIP OR PLUGIN_CHIP STREQUAL "")
  if(DEFINED ENV{AIBOX_PLUGIN_CHIP} AND NOT "$ENV{AIBOX_PLUGIN_CHIP}" STREQUAL "")
    set(PLUGIN_CHIP "$ENV{AIBOX_PLUGIN_CHIP}")
  elseif(DEFINED ENV{AIBOX_PLUGIN_PRODUCT_NAME} AND NOT "$ENV{AIBOX_PLUGIN_PRODUCT_NAME}" STREQUAL "")
    set(PLUGIN_CHIP "$ENV{AIBOX_PLUGIN_PRODUCT_NAME}")
  else()
    set(PLUGIN_CHIP "ax650n")
  endif()
endif()

set(PLUGIN_SO ${PLUGIN_NAME})

# get the timestamp（iso format in seconds）
string(TIMESTAMP PLUGIN_BUILD_TIME "%Y-%m-%dT%H:%M:%SZ" UTC)
# 1 calculate md5
file(MD5 "${PLUGIN_OUTPUT_PATH}/lib${PLUGIN_NAME}.so" PLUGIN_MD5)

# 2 read the template
file(READ
  "${CMAKE_CURRENT_LIST_DIR}/${PLUGIN_NAME}_plugin/config_template.json.in"
  _json_in
)

# 3) use string(CONFIGURE) replace all at once @VAR@
string(CONFIGURE
  "${_json_in}"
  _json_out
  @ONLY
)

# 4)write out the end config.json
file(WRITE
  "${PLUGIN_OUTPUT_PATH}/config.json"
  "${_json_out}"
)
