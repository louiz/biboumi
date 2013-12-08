# - Find Crypto++
# Find the Crypto++ library
#
# This module defines the following variables:
#   CRYPTO++_FOUND  -  True if library and include directory are found
# If set to TRUE, the following are also defined:
#   CRYPTO++_LIBRARIES  -  Where to find the library file
#   CRYPTO++_INCLUDE_DIRS  -  The directory where to find the header files
#
# For conveniance, these variables are also set. They have the same values
# than the variables above.  The user can thus choose his/her prefered way
# to way to write them.
#
#   CRYPTOPP_FOUND
#
#   CRYPTO++_LIBRARY
#   CRYPTOPP_LIBRARY
#   CRYPTOPP_LIBRARIES
#
#   CRYPTO++_INCLUDE_DIR
#   CRYPTOPP_INCLUDE_DIRS
#   CRYPTOPP_INCLUDE_DIR
#
# This file is in the public domain.

find_path(CRYPTO++_INCLUDE_DIRS NAMES cryptlib.h
  PATH_SUFFIXES "crypto++" "cryptopp"
  DOC "The Crypto++ include directory")

find_library(CRYPTO++_LIBRARIES NAMES cryptopp
  DOC "The Crypto++ library")

# Use some standard module to handle the QUIETLY and REQUIRED arguments, and
# set CRYPTO++_FOUND to TRUE if these two variables are set.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Crypto++ REQUIRED_VARS CRYPTO++_LIBRARIES CRYPTO++_INCLUDE_DIRS)

# Compatibility for all the ways of writing these variables
if(CRYPTO++_FOUND)
  set(CRYPTOPP_FOUND ${CRYPTO++_FOUND})

  set(CRYPTO++_LIBRARY ${CRYPTO++_LIBRARIES})
  set(CRYPTOPP_LIBRARY ${CRYPTO++_LIBRARIES})
  set(CRYPTOPP_LIBRARIES ${CRYPTO++_LIBRARIES})

  set(CRYPTO++_INCLUDE_DIR ${CRYPTO++_INCLUDE_DIRS})
  set(CRYPTOPP_INCLUDE_DIR ${CRYPTO++_INCLUDE_DIRS})
  set(CRYPTOPP_INCLUDE_DIRS ${CRYPTO++_INCLUDE_DIRS})
endif()

mark_as_advanced(CRYPTO++_INCLUDE_DIRS CRYPTO++_LIBRARIES)


