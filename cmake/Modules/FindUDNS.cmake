# - Find udns
# Find the udns library
#
# This module defines the following variables:
#   UDNS_FOUND  -  True if library and include directory are found
# If set to TRUE, the following are also defined:
#   UDNS_INCLUDE_DIRS  -  The directory where to find the header file
#   UDNS_LIBRARIES  -  Where to find the library file
#
# For conveniance, these variables are also set. They have the same values
# as the variables above.  The user can thus choose his/her prefered way
# to write them.
#   UDNS_INCLUDE_DIR
#   UDNS_LIBRARY
#
# This file is in the public domain

if(NOT UDNS_FOUND)
  find_path(UDNS_INCLUDE_DIRS NAMES udns.h
    DOC "The udns include directory")

  find_library(UDNS_LIBRARIES NAMES udns
    DOC "The udns library")

  # Use some standard module to handle the QUIETLY and REQUIRED arguments, and
  # set UDNS_FOUND to TRUE if these two variables are set.
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(UDNS REQUIRED_VARS UDNS_LIBRARIES UDNS_INCLUDE_DIRS)

  # Compatibility for all the ways of writing these variables
  if(UDNS_FOUND)
    set(UDNS_INCLUDE_DIR ${UDNS_INCLUDE_DIRS} CACHE INTERNAL "")
    set(UDNS_LIBRARY ${UDNS_LIBRARIES} CACHE INTERNAL "")
    set(UDNS_FOUND ${UDNS_FOUND} CACHE INTERNAL "")
  endif()
endif()

mark_as_advanced(UDNS_INCLUDE_DIRS UDNS_LIBRARIES)
