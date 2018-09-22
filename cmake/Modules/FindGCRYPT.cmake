# - Find gcrypt
# Find the gcrypt cryptographic library
#
# This module defines the following variables:
#   GCRYPT_FOUND  -  True if library and include directory are found
# If set to TRUE, the following are also defined:
#   GCRYPT_INCLUDE_DIRS  -  The directory where to find the header file
#   GCRYPT_LIBRARIES  -  Where to find the library file
#
# For conveniance, these variables are also set. They have the same values
# than the variables above.  The user can thus choose his/her prefered way
# to write them.
#   GCRYPT_LIBRARY
#   GCRYPT_INCLUDE_DIR
#
# This file is in the public domain

include(FindPkgConfig)

if(NOT GCRYPT_FOUND)
  pkg_check_modules(GCRYPT gcrypt)
endif()

if(NOT GCRYPT_FOUND)
  find_path(GCRYPT_INCLUDE_DIRS NAMES gcrypt.h
      PATH_SUFFIXES gcrypt
      DOC "The gcrypt include directory")

  find_library(GCRYPT_LIBRARIES NAMES gcrypt
      DOC "The gcrypt library")

  # Use some standard module to handle the QUIETLY and REQUIRED arguments, and
  # set GCRYPT_FOUND to TRUE if these two variables are set.
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(GCRYPT REQUIRED_VARS GCRYPT_LIBRARIES GCRYPT_INCLUDE_DIRS)

  if(GCRYPT_FOUND)
    set(GCRYPT_LIBRARY ${GCRYPT_LIBRARIES} CACHE INTERNAL "")
    set(GCRYPT_INCLUDE_DIR ${GCRYPT_INCLUDE_DIRS} CACHE INTERNAL "")
    set(GCRYPT_FOUND ${GCRYPT_FOUND} CACHE INTERNAL "")
  endif()
endif()

mark_as_advanced(GCRYPT_INCLUDE_DIRS GCRYPT_LIBRARIES)
