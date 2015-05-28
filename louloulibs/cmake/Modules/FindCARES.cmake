# - Find c-ares
# Find the c-ares library, and more particularly the stringprep header.
#
# This module defines the following variables:
#   CARES_FOUND  -  True if library and include directory are found
# If set to TRUE, the following are also defined:
#   CARES_INCLUDE_DIRS  -  The directory where to find the header file
#   CARES_LIBRARIES  -  Where to find the library file
#
# For conveniance, these variables are also set. They have the same values
# than the variables above.  The user can thus choose his/her prefered way
# to write them.
#   CARES_INCLUDE_DIR
#   CARES_LIBRARY
#
# This file is in the public domain

if(NOT CARES_FOUND)
  find_path(CARES_INCLUDE_DIRS NAMES ares.h
    DOC "The c-ares include directory")

  find_library(CARES_LIBRARIES NAMES cares
    DOC "The c-ares library")

  # Use some standard module to handle the QUIETLY and REQUIRED arguments, and
  # set CARES_FOUND to TRUE if these two variables are set.
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(CARES REQUIRED_VARS CARES_LIBRARIES CARES_INCLUDE_DIRS)

  # Compatibility for all the ways of writing these variables
  if(CARES_FOUND)
    set(CARES_INCLUDE_DIR ${CARES_INCLUDE_DIRS})
    set(CARES_LIBRARY ${CARES_LIBRARIES})
  endif()
endif()

mark_as_advanced(CARES_INCLUDE_DIRS CARES_LIBRARIES)
