#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "cjson" for configuration "Release"
set_property(TARGET cjson APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(cjson PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "/usr/local/lib/libcjson.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS cjson )
list(APPEND _IMPORT_CHECK_FILES_FOR_cjson "/usr/local/lib/libcjson.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
