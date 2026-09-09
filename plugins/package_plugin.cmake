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
set(RESOLVED_LICENSE_DIR "${DEFAULT_LICENSE_DIR}")
set(RESOLVED_PLUGIN_CHIP "")
set(RESOLVED_PLUGIN_ARCH "")
set(RESOLVED_PLUGIN_OS "")

# Keep the build tree and signing scope coupled.  A plugin DSO is portable
# across the aarch64 products, but an encrypted package is not: the loader
# rejects a package signed for another product before the node factory can be
# registered.  Infer only from an unambiguous build-directory marker; generic
# build directories continue to use the existing explicit/default selection.
set(INFERRED_BUILD_CHIP "")
if("${PLUGIN_LIBRARY}" MATCHES "/build[^/]*rk3588[^/]*/")
  set(INFERRED_BUILD_CHIP "rk3588")
endif()

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
  # Resource class comes from the package directory, never its file extension.
  # configs remains readable while source trees move to the singular config name.
  foreach(resource_name IN ITEMS lib config configs data models)
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
elseif(NOT "${INFERRED_BUILD_CHIP}" STREQUAL "")
  set(RESOLVED_PLUGIN_CHIP "${INFERRED_BUILD_CHIP}")
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

if(NOT "${INFERRED_BUILD_CHIP}" STREQUAL "" AND
   NOT "${RESOLVED_PLUGIN_CHIP}" STREQUAL "${INFERRED_BUILD_CHIP}")
  message(FATAL_ERROR
    "Plugin target/build-tree mismatch: library '${PLUGIN_LIBRARY}' belongs "
    "to '${INFERRED_BUILD_CHIP}', but packaging selected "
    "'${RESOLVED_PLUGIN_CHIP}'. Use the matching AIBOX_PLUGIN_CHIP and "
    "AIBOX_PLUGIN_LICENSE_DIR.")
endif()

# Packaging may keep licenses for several product families side by side.  The
# generic .license symlink is a developer convenience and must never silently
# override an explicit target chip in a release build.
if(DEFINED ENV{AIBOX_PLUGIN_LICENSE_DIR} AND NOT "$ENV{AIBOX_PLUGIN_LICENSE_DIR}" STREQUAL "")
  set(RESOLVED_LICENSE_DIR "$ENV{AIBOX_PLUGIN_LICENSE_DIR}")
elseif(NOT "${RESOLVED_PLUGIN_CHIP}" STREQUAL "" AND
       EXISTS "${CMAKE_CURRENT_LIST_DIR}/.license.${RESOLVED_PLUGIN_CHIP}/license.json")
  set(RESOLVED_LICENSE_DIR "${CMAKE_CURRENT_LIST_DIR}/.license.${RESOLVED_PLUGIN_CHIP}")
endif()

if(NOT EXISTS "${RESOLVED_LICENSE_DIR}/license.json")
  message(FATAL_ERROR "Plugin license metadata not found for target '${RESOLVED_PLUGIN_CHIP}': ${RESOLVED_LICENSE_DIR}/license.json")
endif()

if(DEFINED ENV{AIBOX_PLUGIN_ARCH} AND NOT "$ENV{AIBOX_PLUGIN_ARCH}" STREQUAL "")
  set(RESOLVED_PLUGIN_ARCH "$ENV{AIBOX_PLUGIN_ARCH}")
else()
  # This packaging project always emits AX650 runtime libraries. The pack tool
  # runs on an x86_64 build server, so uname would describe the host rather
  # than the plugin ELF target.
  set(RESOLVED_PLUGIN_ARCH "aarch64")
endif()

if(DEFINED ENV{AIBOX_PLUGIN_OS} AND NOT "$ENV{AIBOX_PLUGIN_OS}" STREQUAL "")
  set(RESOLVED_PLUGIN_OS "$ENV{AIBOX_PLUGIN_OS}")
else()
  set(RESOLVED_PLUGIN_OS "Linux")
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

# Sanitizer DSOs are diagnostic artifacts.  A previous ASan build and the
# production build shared build_out/<name>.plugin, so the diagnostic package
# silently replaced the release package.  Reject that state at the common
# packager boundary; diagnostic CMake targets must use build_out/diagnostic/.
file(STRINGS "${PLUGIN_LIBRARY}" _sanitizer_runtime_refs
     REGEX "lib(asan|ubsan)\\.so")
if(_sanitizer_runtime_refs AND
   PACKAGE_OUTPUT MATCHES "/build_out/[^/]+\\.plugin$")
  _fail_and_cleanup(
    "Refusing to publish sanitizer plugin '${PLUGIN_NAME}' to production output: ${PACKAGE_OUTPUT}. Use build_out/diagnostic instead.")
endif()

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
          -DPLUGIN_CONFIG_TEMPLATE=${PLUGIN_CONFIG_TEMPLATE}
          -P "${GENERATE_CONFIG_SCRIPT}"
  RESULT_VARIABLE config_result
  ERROR_VARIABLE config_error
)
if(NOT config_result EQUAL 0)
  _fail_and_cleanup("Failed to generate config.json for ${PLUGIN_NAME}: ${config_error}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
          "AIBOX_LICENSE_DIR=${RESOLVED_LICENSE_DIR}"
          "${PACK_TOOL}" -C "${STAGING_DIR}" -o "${PACKAGE_OUTPUT}"
  RESULT_VARIABLE pack_result
  ERROR_VARIABLE pack_error
)
if(NOT pack_result EQUAL 0)
  _fail_and_cleanup("Failed to package ${PLUGIN_NAME}: ${pack_error}")
endif()

# Treat the archive metadata as the final authority.  This catches a stale or
# incorrectly pointed license directory even when the requested chip value was
# otherwise correct, before the package reaches a device and causes every task
# using that node type to fail recovery.
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
          "AIBOX_LICENSE_DIR=${RESOLVED_LICENSE_DIR}"
          "${PACK_TOOL}" info -i "${PACKAGE_OUTPUT}" -json
  RESULT_VARIABLE info_result
  OUTPUT_VARIABLE package_info_json
  ERROR_VARIABLE info_error
)
if(NOT info_result EQUAL 0)
  _fail_and_cleanup(
    "Failed to audit packaged plugin ${PLUGIN_NAME}: ${info_error}")
endif()

foreach(metadata_key IN ITEMS target architecture os name type)
  string(JSON package_${metadata_key}
         ERROR_VARIABLE package_${metadata_key}_error
         GET "${package_info_json}" "${metadata_key}")
  if(package_${metadata_key}_error)
    _fail_and_cleanup(
      "Packaged plugin ${PLUGIN_NAME} has no valid '${metadata_key}' metadata: "
      "${package_${metadata_key}_error}")
  endif()
endforeach()

if(NOT "${RESOLVED_PLUGIN_CHIP}" STREQUAL "" AND
   NOT "${package_target}" STREQUAL "${RESOLVED_PLUGIN_CHIP}")
  _fail_and_cleanup(
    "Packaged plugin target mismatch: expected '${RESOLVED_PLUGIN_CHIP}', "
    "got '${package_target}'")
endif()
if(NOT "${package_architecture}" STREQUAL "${RESOLVED_PLUGIN_ARCH}")
  _fail_and_cleanup(
    "Packaged plugin architecture mismatch: expected '${RESOLVED_PLUGIN_ARCH}', "
    "got '${package_architecture}'")
endif()
if(NOT "${package_os}" STREQUAL "${RESOLVED_PLUGIN_OS}")
  _fail_and_cleanup(
    "Packaged plugin OS mismatch: expected '${RESOLVED_PLUGIN_OS}', "
    "got '${package_os}'")
endif()
if(NOT "${package_name}" STREQUAL "${PLUGIN_NAME}" OR
   NOT "${package_type}" STREQUAL "${PLUGIN_NAME}")
  _fail_and_cleanup(
    "Packaged plugin identity mismatch: expected '${PLUGIN_NAME}', got "
    "name='${package_name}' type='${package_type}'")
endif()

message(STATUS
  "Plugin package audit passed: name=${package_name} target=${package_target} "
  "arch=${package_architecture} os=${package_os}")

_cleanup_staging()
