# - Find SystemdDaemon
# Find the systemd daemon library
#
# This module defines the following variables:
#   SYSTEMDDAEMON_FOUND  -  True if library and include directory are found
# If set to TRUE, the following are also defined:
#   SYSTEMDDAEMON_INCLUDE_DIRS  -  The directory where to find the header file
#   SYSTEMDDAEMON_LIBRARIES  -  Where to find the library file
#
# For conveniance, these variables are also set. They have the same values
# than the variables above.  The user can thus choose his/her prefered way
# to write them.
#   SYSTEMDDAEMON_LIBRARY
#   SYSTEMDDAEMON_INCLUDE_DIR
#
# This file is in the public domain

include(FindPkgConfig)
pkg_check_modules(SYSTEMDDAEMON libsystemd-daemon)

if(NOT SYSTEMDDAEMON_FOUND)
  find_path(SYSTEMDDAEMON_INCLUDE_DIRS NAMES systemd/sd-daemon.h
    DOC "The Systemd Daemon include directory")

  find_library(SYSTEMDDAEMON_LIBRARIES NAMES systemd-daemon systemd
    DOC "The Systemd Daemon library")

  # Use some standard module to handle the QUIETLY and REQUIRED arguments, and
  # set SYSTEMDDAEMON_FOUND to TRUE if these two variables are set.
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(SYSTEMDDAEMON REQUIRED_VARS SYSTEMDDAEMON_LIBRARIES SYSTEMDDAEMON_INCLUDE_DIRS)

  if(SYSTEMDDAEMON_FOUND)
    set(SYSTEMDDAEMON_LIBRARY ${SYSTEMDDAEMON_LIBRARIES})
    set(SYSTEMDDAEMON_INCLUDE_DIR ${SYSTEMDDAEMON_INCLUDE_DIRS})
  endif()
endif()

mark_as_advanced(SYSTEMDDAEMON_INCLUDE_DIRS SYSTEMDDAEMON_LIBRARIES)