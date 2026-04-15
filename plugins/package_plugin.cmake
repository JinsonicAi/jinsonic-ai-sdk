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
set(DEFAULT_LICENSE_DIR "${CMAKE_CURRENT_LIST_DIR}/.license")
set(RESOLVED_PLUGIN_CHIP "")
set(RESOLVED_PLUGIN_ARCH "")
set(RESOLVED_PLUGIN_OS "")

set(PLUGIN_SOURCE_DIR "")
foreach(candidate IN ITEMS
    "${CMAKE_CURRENT_LIST_DIR}/${PLUGIN_NAME}_plugin"
    "${CMAKE_CURRENT_LIST_DIR}/${PLUGIN_NAME}-plugin"
    "${CMAKE_CURRENT_LIST_DIR}/${PLUGIN_NAME}")
  if(EXISTS "${candidate}/config_template.json.in")
    set(PLUGIN_SOURCE_DIR "${candidate}")
    break()
  endif()
endforeach()

if(PLUGIN_SOURCE_DIR)
  foreach(resource_name IN ITEMS lib configs models)
    set(resource_dir "${PLUGIN_SOURCE_DIR}/${resource_name}")
    if(EXISTS "${resource_dir}" AND IS_DIRECTORY "${resource_dir}")
      list(APPEND PLUGIN_RESOURCE_DIRS "${resource_dir}")
    endif()
  endforeach()
endif()

if(PLUGIN_RESOURCE_DIRS)
  list(REMOVE_DUPLICATES PLUGIN_RESOURCE_DIRS)
endif()

if(DEFINED ENV{AIBOX_PLUGIN_CHIP} AND NOT "$ENV{AIBOX_PLUGIN_CHIP}" STREQUAL "")
  set(RESOLVED_PLUGIN_CHIP "$ENV{AIBOX_PLUGIN_CHIP}")
elseif(DEFINED ENV{AIBOX_PLUGIN_PRODUCT_NAME} AND NOT "$ENV{AIBOX_PLUGIN_PRODUCT_NAME}" STREQUAL "")
  set(RESOLVED_PLUGIN_CHIP "$ENV{AIBOX_PLUGIN_PRODUCT_NAME}")
else()
  set(LICENSE_JSON_PATH "${DEFAULT_LICENSE_DIR}/license.json")
  if(EXISTS "${LICENSE_JSON_PATH}")
    file(READ "${LICENSE_JSON_PATH}" _license_json_raw)
    string(JSON _license_build_type ERROR_VARIABLE _license_build_type_err TYPE "${_license_json_raw}" build)
    if(NOT _license_build_type_err)
      string(JSON _license_product_name ERROR_VARIABLE _license_product_name_err GET "${_license_json_raw}" build product_name)
      if(NOT _license_product_name_err AND NOT "${_license_product_name}" STREQUAL "")
        set(RESOLVED_PLUGIN_CHIP "${_license_product_name}")
      endif()
    endif()
  endif()
endif()

if(DEFINED ENV{AIBOX_PLUGIN_ARCH} AND NOT "$ENV{AIBOX_PLUGIN_ARCH}" STREQUAL "")
  set(RESOLVED_PLUGIN_ARCH "$ENV{AIBOX_PLUGIN_ARCH}")
else()
  execute_process(COMMAND uname -m OUTPUT_VARIABLE RESOLVED_PLUGIN_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

if(DEFINED ENV{AIBOX_PLUGIN_OS} AND NOT "$ENV{AIBOX_PLUGIN_OS}" STREQUAL "")
  set(RESOLVED_PLUGIN_OS "$ENV{AIBOX_PLUGIN_OS}")
else()
  execute_process(COMMAND uname -s OUTPUT_VARIABLE RESOLVED_PLUGIN_OS OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

if(EXISTS "${PACK_TOOL}")
  file(CHMOD "${PACK_TOOL}"
       PERMISSIONS
         OWNER_READ OWNER_WRITE OWNER_EXECUTE
         GROUP_READ GROUP_EXECUTE
         WORLD_READ WORLD_EXECUTE)
endif()

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
          -DPLUGIN_CHIP=${RESOLVED_PLUGIN_CHIP}
          -DPLUGIN_ARCH=${RESOLVED_PLUGIN_ARCH}
          -DPLUGIN_OS=${RESOLVED_PLUGIN_OS}
          -P "${GENERATE_CONFIG_SCRIPT}"
  RESULT_VARIABLE config_result
  ERROR_VARIABLE config_error
)
if(NOT config_result EQUAL 0)
  _fail_and_cleanup("Failed to generate config.json for ${PLUGIN_NAME}: ${config_error}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
          "AIBOX_LICENSE_DIR=${DEFAULT_LICENSE_DIR}"
          "${PACK_TOOL}" -C "${STAGING_DIR}" -o "${PACKAGE_OUTPUT}"
  RESULT_VARIABLE pack_result
  ERROR_VARIABLE pack_error
)
if(NOT pack_result EQUAL 0)
  _fail_and_cleanup("Failed to package ${PLUGIN_NAME}: ${pack_error}")
endif()

_cleanup_staging()
