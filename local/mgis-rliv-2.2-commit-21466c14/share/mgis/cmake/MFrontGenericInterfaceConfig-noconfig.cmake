#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "mgis::MFrontGenericInterface" for configuration ""
set_property(TARGET mgis::MFrontGenericInterface APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(mgis::MFrontGenericInterface PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libMFrontGenericInterface.so"
  IMPORTED_SONAME_NOCONFIG "libMFrontGenericInterface.so"
  )

list(APPEND _cmake_import_check_targets mgis::MFrontGenericInterface )
list(APPEND _cmake_import_check_files_for_mgis::MFrontGenericInterface "${_IMPORT_PREFIX}/lib/libMFrontGenericInterface.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
