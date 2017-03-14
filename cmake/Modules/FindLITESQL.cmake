# - Find LiteSQL
#
# Find the LiteSQL library, and defines a function to generate C++ files
# from the database xml file using litesql-gen fro
#
# This module defines the following variables:
#   LITESQL_FOUND  -  True if library and include directory are found
# If set to TRUE, the following are also defined:
#   LITESQL_INCLUDE_DIRS  -  The directory where to find the header file
#   LITESQL_LIBRARIES  -  Where to find the library file
#   LITESQL_GENERATE_CPP - A function, to be used like this:
# LITESQL_GENERATE_CPP("db/database.xml"  # The file defining the db schemas
#                      "database"         # The name of the C++ “module”
#                                         # that will be generated
#                       LITESQL_GENERATED_SOURCES # Variable containing the
#                                                 resulting C++ files to compile
#
# For conveniance, these variables are also set. They have the same values
# than the variables above.  The user can thus choose his/her prefered way
# to write them.
#   LITESQL_INCLUDE_DIR
#   LITESQL_LIBRARY
#
# This file is in the public domain

find_path(LITESQL_INCLUDE_DIRS NAMES litesql.hpp
  DOC "The LiteSQL include directory")

find_library(LITESQL_LIBRARIES NAMES litesql
  DOC "The LiteSQL library")

foreach(DB_TYPE sqlite postgresql mysql ocilib)
  string(TOUPPER ${DB_TYPE} DB_TYPE_UPPER)
  find_library(LITESQL_${DB_TYPE_UPPER}_LIB_PATH NAMES litesql_${DB_TYPE}
    DOC "The ${DB_TYPE} backend for LiteSQL")
  if(LITESQL_${DB_TYPE_UPPER}_LIB_PATH)
    list(APPEND LITESQL_LIBRARIES ${LITESQL_${DB_TYPE_UPPER}_LIB_PATH})
  endif()
  mark_as_advanced(LITESQL_${DB_TYPE_UPPER}_LIB_PATH)
endforeach()

find_program(LITESQLGEN_EXECUTABLE NAMES litesql-gen
  DOC "The utility that creates .h and .cpp files from a xml database description")

# Use some standard module to handle the QUIETLY and REQUIRED arguments, and
# set LITESQL_FOUND to TRUE if these two variables are set.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LITESQL REQUIRED_VARS LITESQL_LIBRARIES LITESQL_INCLUDE_DIRS
  LITESQLGEN_EXECUTABLE)

# Compatibility for all the ways of writing these variables
if(LITESQL_FOUND)
  set(LITESQL_INCLUDE_DIR ${LITESQL_INCLUDE_DIRS})
  set(LITESQL_LIBRARY ${LITESQL_LIBRARIES})
endif()

mark_as_advanced(LITESQL_INCLUDE_DIRS LITESQL_LIBRARIES LITESQLGEN_EXECUTABLE)


# LITESQL_GENERATE_CPP function

function(LITESQL_GENERATE_CPP
    SOURCE_FILE OUTPUT_NAME OUTPUT_SOURCES)
  set(${OUTPUT_SOURCES})
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_NAME}.cpp"
           "${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_NAME}.hpp"
    COMMAND  ${LITESQLGEN_EXECUTABLE}
    ARGS -t c++ --output-dir=${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE}
    DEPENDS ${SOURCE_FILE}
    COMMENT "Running litesql-gen on ${SOURCE_FILE}"
    VERBATIM)
  list(APPEND ${OUTPUT_SOURCES} "${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_NAME}.cpp")
  set_source_files_properties(${${OUTPUT_SOURCES}} PROPERTIES GENERATED TRUE)
  set(${OUTPUT_SOURCES} ${${OUTPUT_SOURCES}} PARENT_SCOPE)
endfunction()
