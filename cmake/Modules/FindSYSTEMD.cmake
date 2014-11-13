# - Find SystemdDaemon
# Find the systemd daemon library
#
# This module defines the following variables:
#   SYSTEMD_FOUND  -  True if library and include directory are found
# If set to TRUE, the following are also defined:
#   SYSTEMD_INCLUDE_DIRS  -  The directory where to find the header file
#   SYSTEMD_LIBRARIES  -  Where to find the library file
#
# For conveniance, these variables are also set. They have the same values
# than the variables above.  The user can thus choose his/her prefered way
# to write them.
#   SYSTEMD_LIBRARY
#   SYSTEMD_INCLUDE_DIR
#
# This file is in the public domain

include(FindPkgConfig)
pkg_check_modules(SYSTEMD libsystemd)

if(NOT SYSTEMD_FOUND)
  find_path(SYSTEMD_INCLUDE_DIRS NAMES systemd/sd-daemon.h
    DOC "The Systemd include directory")

  find_library(SYSTEMD_LIBRARIES NAMES systemd
    DOC "The Systemd library")

  # Use some standard module to handle the QUIETLY and REQUIRED arguments, and
  # set SYSTEMD_FOUND to TRUE if these two variables are set.
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(SYSTEMD REQUIRED_VARS SYSTEMD_LIBRARIES SYSTEMD_INCLUDE_DIRS)

  if(SYSTEMD_FOUND)
    set(SYSTEMD_LIBRARY ${SYSTEMD_LIBRARIES})
    set(SYSTEMD_INCLUDE_DIR ${SYSTEMD_INCLUDE_DIRS})
  endif()
endif()

mark_as_advanced(SYSTEMD_INCLUDE_DIRS SYSTEMD_LIBRARIES)