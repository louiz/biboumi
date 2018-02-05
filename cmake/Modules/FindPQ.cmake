# - Find libpq
# Find the postgresql front end library
#
# This module defines the following variables:
#   PQ_FOUND  -  True if library and include directory are found
# If set to TRUE, the following are also defined:
#   PQ_INCLUDE_DIRS  -  The directory where to find the header file
#   PQ_LIBRARIES  -  Where to find the library file
#
# For conveniance, these variables are also set. They have the same values
# than the variables above.  The user can thus choose his/her prefered way
# to write them.
#   PQ_LIBRARY
#   PQ_INCLUDE_DIR
#
# This file is in the public domain

include(FindPkgConfig)

if(NOT PQ_FOUND)
  pkg_check_modules(PQ libpq)
endif()

if(NOT PQ_FOUND)
  find_path(PQ_INCLUDE_DIRS NAMES libpq-fe.h
      DOC "The libpq include directory")

  find_library(PQ_LIBRARIES NAMES pq
      DOC "The pq library")

  # Use some standard module to handle the QUIETLY and REQUIRED arguments, and
  # set PQ_FOUND to TRUE if these two variables are set.
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(PQ REQUIRED_VARS PQ_LIBRARIES PQ_INCLUDE_DIRS)

  if(PQ_FOUND)
    set(PQ_LIBRARY ${PQ_LIBRARIES} CACHE INTERNAL "")
    set(PQ_INCLUDE_DIR ${PQ_INCLUDE_DIRS} CACHE INTERNAL "")
    set(PQ_FOUND ${PQ_FOUND} CACHE INTERNAL "")
  endif()
endif()

mark_as_advanced(PQ_INCLUDE_DIRS PQ_LIBRARIES)
