# - Find sqlite3
# Find the sqlite3 cryptographic library
#
# This module defines the following variables:
#   SQLITE3_FOUND  -  True if library and include directory are found
# If set to TRUE, the following are also defined:
#   SQLITE3_INCLUDE_DIRS  -  The directory where to find the header file
#   SQLITE3_LIBRARIES  -  Where to find the library file
#
# For conveniance, these variables are also set. They have the same values
# than the variables above.  The user can thus choose his/her prefered way
# to write them.
#   SQLITE3_LIBRARY
#   SQLITE3_INCLUDE_DIR
#
# This file is in the public domain

include(FindPkgConfig)

if(NOT SQLITE3_FOUND)
    pkg_check_modules(SQLITE3 sqlite3)
endif()

if(NOT SQLITE3_FOUND)
    find_path(SQLITE3_INCLUDE_DIRS NAMES sqlite3.h
            DOC "The sqlite3 include directory")

    find_library(SQLITE3_LIBRARIES NAMES sqlite3
            DOC "The sqlite3 library")

    # Use some standard module to handle the QUIETLY and REQUIRED arguments, and
    # set SQLITE3_FOUND to TRUE if these two variables are set.
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(SQLITE3 REQUIRED_VARS SQLITE3_LIBRARIES SQLITE3_INCLUDE_DIRS)

    if(SQLITE3_FOUND)
        set(SQLITE3_LIBRARY ${SQLITE3_LIBRARIES} CACHE INTERNAL "")
        set(SQLITE3_INCLUDE_DIR ${SQLITE3_INCLUDE_DIRS} CACHE INTERNAL "")
        set(SQLITE3_FOUND ${SQLITE3_FOUND} CACHE INTERNAL "")
    endif()
endif()

mark_as_advanced(SQLITE3_INCLUDE_DIRS SQLITE3_LIBRARIES)