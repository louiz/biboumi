# - Find iconv
# Find the iconv (character set conversion) library
#
# This module defines the following variables:
#   ICONV_FOUND  -  True if library and include directory are found
# If set to TRUE, the following are also defined:
#   ICONV_INCLUDE_DIRS  -  The directory where to find the header file
#   ICONV_LIBRARIES  -  Where to find the library file
#   ICONV_SECOND_ARGUMENT_IS_CONST  -  The second argument for iconv() is const
#
# For conveniance, these variables are also set. They have the same values
# than the variables above.  The user can thus choose his/her prefered way
# to write them.
#   ICONV_LIBRARY
#   ICONV_INCLUDE_DIR
#
# This file is in the public domain

find_path(ICONV_INCLUDE_DIRS NAMES iconv.h
  DOC "The iconv include directory")

find_library(ICONV_LIBRARIES NAMES iconv libiconv libiconv-2 c
  DOC "The iconv library")

# Use some standard module to handle the QUIETLY and REQUIRED arguments, and
# set ICONV_FOUND to TRUE if these two variables are set.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Iconv REQUIRED_VARS ICONV_LIBRARIES ICONV_INCLUDE_DIRS)

# Check if the prototype is
# size_t iconv(iconv_t cd, char** inbuf, size_t* inbytesleft,
#              char** outbuf, size_t* outbytesleft);
# or
# size_t iconv (iconv_t cd, const char** inbuf, size_t* inbytesleft,
#              char** outbuf, size_t* outbytesleft);
if(ICONV_FOUND)
  include(CheckCXXSourceCompiles)

  # Set the parameters needed to compile the following code.
  set(CMAKE_REQUIRED_INCLUDES ${ICONV_INCLUDE_DIRS})
  set(CMAKE_REQUIRED_LIBRARIES ${ICONV_LIBRARIES})

 check_cxx_source_compiles("
   #include <iconv.h>
   int main(){
   iconv_t conv = 0;
   const char* in = 0;
   size_t ilen = 0;
   char* out = 0;
   size_t olen = 0;
   iconv(conv, &in, &ilen, &out, &olen);
   return 0;}"
   ICONV_SECOND_ARGUMENT_IS_CONST)

# Compatibility for all the ways of writing these variables
  set(ICONV_LIBRARY ${ICONV_LIBRARIES})
  set(ICONV_INCLUDE_DIR ${ICONV_INCLUDE_DIRS})
endif()

mark_as_advanced(ICONV_INCLUDE_DIRS ICONV_LIBRARIES ICONV_SECOND_ARGUMENT_IS_CONST)