# - Find libidn
# Find the libidn library, and more particularly the stringprep header.
#
# This module defines the following variables:
#   LIBIDN_FOUND  -  True if library and include directory are found
# If set to TRUE, the following are also defined:
#   LIBIDN_INCLUDE_DIRS  -  The directory where to find the header file
#   LIBIDN_LIBRARIES  -  Where to find the library file
#
# For conveniance, these variables are also set. They have the same values
# than the variables above.  The user can thus choose his/her prefered way
# to write them.
#   LIBIDN_INCLUDE_DIR
#   LIBIDN_LIBRARY
#
# This file is in the public domain

include(FindPkgConfig)
pkg_check_modules(LIBIDN libidn)

if(NOT LIBIDN_FOUND)
  find_path(LIBIDN_INCLUDE_DIRS NAMES stringprep.h
    DOC "The libidn include directory")

  # The library containing the stringprep module is libidn
  find_library(LIBIDN_LIBRARIES NAMES idn
    DOC "The libidn library")

  # Use some standard module to handle the QUIETLY and REQUIRED arguments, and
  # set LIBIDN_FOUND to TRUE if these two variables are set.
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(LIBIDN REQUIRED_VARS LIBIDN_LIBRARIES LIBIDN_INCLUDE_DIRS)

  # Compatibility for all the ways of writing these variables
  if(LIBIDN_FOUND)
    set(LIBIDN_INCLUDE_DIR ${LIBIDN_INCLUDE_DIRS})
    set(LIBIDN_LIBRARY ${LIBIDN_LIBRARIES})
  endif()
endif()

mark_as_advanced(LIBIDN_INCLUDE_DIRS LIBIDN_LIBRARIES)