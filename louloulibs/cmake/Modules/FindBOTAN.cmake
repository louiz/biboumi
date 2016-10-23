# - Find botan
# Find the botan cryptographic library
#
# This module defines the following variables:
#   BOTAN_FOUND  -  True if library and include directory are found
# If set to TRUE, the following are also defined:
#   BOTAN_INCLUDE_DIRS  -  The directory where to find the header file
#   BOTAN_LIBRARIES  -  Where to find the library file
#
# For conveniance, these variables are also set. They have the same values
# than the variables above.  The user can thus choose his/her prefered way
# to write them.
#   BOTAN_LIBRARY
#   BOTAN_INCLUDE_DIR
#
# This file is in the public domain

find_path(BOTAN_INCLUDE_DIRS NAMES botan/botan.h
  PATH_SUFFIXES botan-1.11
  DOC "The botan include directory")

find_library(BOTAN_LIBRARIES NAMES botan botan-1.11
  DOC "The botan library")

# Use some standard module to handle the QUIETLY and REQUIRED arguments, and
# set BOTAN_FOUND to TRUE if these two variables are set.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(BOTAN REQUIRED_VARS BOTAN_LIBRARIES BOTAN_INCLUDE_DIRS)

if(BOTAN_FOUND)
  set(BOTAN_LIBRARY ${BOTAN_LIBRARIES})
  set(BOTAN_INCLUDE_DIR ${BOTAN_INCLUDE_DIRS})
endif()

mark_as_advanced(BOTAN_INCLUDE_DIRS BOTAN_LIBRARIES)
