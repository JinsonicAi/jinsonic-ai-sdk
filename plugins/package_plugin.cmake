if(NOT DEFINED PLUGIN_NAME)
  message(FATAL_ERROR "PLUGIN_NAME is required")
endif()

if(NOT DEFINED PLUGIN_LIBRARY)
  message(FATAL_ERROR "PLUGIN_LIBRARY is required")
endif()

if(NOT DEFINED PACK_TOOL)
  message(FATAL_ERROR "PACK_TOOL is required")
endif()

if(NOT DEFINED PACKAGE_OUTPUT)
  message(FATAL_ERROR "PACKAGE_OUTPUT is required")
endif()

if(NOT DEFINED GENERATE_CONFIG_SCRIPT)
  message(FATAL_ERROR "GENERATE_CONFIG_SCRIPT is required")
endif()

if(NOT DEFINED STAGING_DIR)
  message(FATAL_ERROR "STAGING_DIR is required")
endif()

set(PLUGIN_RESOURCE_DIRS "${PLUGIN_RESOURCE_DIRS}")

function(_cleanup_staging)
  if(EXISTS "${STAGING_DIR}")
    file(REMOVE_RECURSE "${STAGING_DIR}")
  endif()
endfunction()

function(_fail_and_cleanup reason)
  _cleanup_staging()
  message(FATAL_ERROR "${reason}")
endfunction()

_cleanup_staging()
file(MAKE_DIRECTORY "${STAGING_DIR}")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${PLUGIN_LIBRARY}" "${STAGING_DIR}/lib${PLUGIN_NAME}.so"
  RESULT_VARIABLE copy_result
  ERROR_VARIABLE copy_error
)
if(NOT copy_result EQUAL 0)
  _fail_and_cleanup("Failed to stage lib${PLUGIN_NAME}.so: ${copy_error}")
endif()

foreach(resource_dir IN LISTS PLUGIN_RESOURCE_DIRS)
  if(NOT resource_dir STREQUAL "")
    get_filename_component(resource_name "${resource_dir}" NAME)
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E copy_directory "${resource_dir}" "${STAGING_DIR}/${resource_name}"
      RESULT_VARIABLE resource_result
      ERROR_VARIABLE resource_error
    )
    if(NOT resource_result EQUAL 0)
      _fail_and_cleanup("Failed to stage resource ${resource_dir}: ${resource_error}")
    endif()
  endif()
endforeach()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
          -DPLUGIN_OUTPUT_PATH=${STAGING_DIR}
          -DPLUGIN_NAME=${PLUGIN_NAME}
          -P "${GENERATE_CONFIG_SCRIPT}"
  RESULT_VARIABLE config_result
  ERROR_VARIABLE config_error
)
if(NOT config_result EQUAL 0)
  _fail_and_cleanup("Failed to generate config.json for ${PLUGIN_NAME}: ${config_error}")
endif()

execute_process(
  COMMAND "${PACK_TOOL}" -C "${STAGING_DIR}" -o "${PACKAGE_OUTPUT}"
  RESULT_VARIABLE pack_result
  ERROR_VARIABLE pack_error
)
if(NOT pack_result EQUAL 0)
  _fail_and_cleanup("Failed to package ${PLUGIN_NAME}: ${pack_error}")
endif()

_cleanup_staging()
