# generate_config.cmake
# Detect arch
# optional read the chip identification can be passed in or automatically detected
# execute_process(COMMAND uname -m OUTPUT_VARIABLE PLUGIN_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND uname -s OUTPUT_VARIABLE PLUGIN_OS OUTPUT_STRIP_TRAILING_WHITESPACE)
set(PLUGIN_ARCH "aarch64")
set(PLUGIN_VERSION "3.0.2")
set(PLUGIN_CHIP "ax650n")
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
