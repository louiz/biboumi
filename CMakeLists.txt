cmake_minimum_required(VERSION 2.6)

project(biboumi)

set(${PROJECT_NAME}_VERSION_MAJOR 0)
set(${PROJECT_NAME}_VERSION_MINOR 1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pedantic -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Og")

# Define a __FILENAME__ macro to get the filename of each file, instead of
# the full path as in __FILE__
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__FILENAME__='\"$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"'")

#
## Look for external libraries
#
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
find_package(Cryptopp REQUIRED)
find_package(Iconv REQUIRED)
include(FindEXPAT)
find_package(EXPAT REQUIRED)

include_directories("src/")
include_directories(${EXPAT_INCLUDE_DIRS})
include_directories(${ICONV_INCLUDE_DIR})
# the SYSTEM flag tells the compiler that we don't care about warnings
# coming from these headers.
include_directories(SYSTEM ${CRYPTO++_INCLUDE_DIR})

set(POLLER "POLL" CACHE STRING
  "Choose the poller between POLL and EPOLL (Linux-only)")
if((NOT ${POLLER} MATCHES "POLL") AND
    (NOT ${POLLER} MATCHES "EPOLL"))
  message(FATAL_ERROR "POLLER must be either POLL or EPOLL")
endif()
#
## utils
#
file(GLOB source_utils
  src/utils/*.[hc]pp)
add_library(utils STATIC ${source_utils})
target_link_libraries(utils ${ICONV_LIBRARIES})

#
## config
#
file(GLOB source_config
  src/config/*.[hc]pp)
add_library(config STATIC ${source_config})
target_link_libraries(config utils)

#
## logger
#
file(GLOB source_logger
  src/logger/*.[hc]pp)
add_library(logger STATIC ${source_logger})
target_link_libraries(logger config)

#
## network
#
file(GLOB source_network
  src/network/*.[hc]pp)
add_library(network STATIC ${source_network})

#
## irclib
#
file(GLOB source_irc
  src/irc/*.[hc]pp)
add_library(irc STATIC ${source_irc})
target_link_libraries(irc network utils)

#
## xmpplib
#
file(GLOB source_xmpp
  src/xmpp/*.[hc]pp)
add_library(xmpp STATIC ${source_xmpp})
target_link_libraries(xmpp bridge network utils
  ${CRYPTO++_LIBRARIES} ${EXPAT_LIBRARIES} pthread)

#
## bridge
#
file(GLOB source_bridge
  src/bridge/*.[hc]pp)
add_library(bridge STATIC ${source_bridge})
target_link_libraries(bridge xmpp irc utils)

#
## Main executable
#
add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME}
  xmpp
  irc
  bridge
  utils
  config)

#
## Tests
#

add_executable(test src/test.cpp)
target_link_libraries(test
  xmpp
  irc
  bridge
  utils
  config
  logger)

configure_file(config.h.cmake src/config.h)
