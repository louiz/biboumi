cmake_minimum_required(VERSION 2.6)

project(biboumi)

set(${PROJECT_NAME}_VERSION_MAJOR 0)
set(${PROJECT_NAME}_VERSION_MINOR 1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pedantic -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Og -fsanitize=address")

#
## Look for external libraries
#
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
find_package(Cryptopp REQUIRED)

include_directories("src/")
# the SYSTEM flag tells the compiler that we don't care about warnings
# coming from these headers.
include_directories(SYSTEM ${CRYPTO++_INCLUDE_DIR})

#
## utils
#
file(GLOB source_utils
  src/utils/*.[hc]pp)
add_library(utils STATIC ${source_utils})

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
target_link_libraries(xmpp bridge network utils ${CRYPTO++_LIBRARIES} expatpp)

#
## bridge
#
file(GLOB source_bridge
  src/bridge/*.[hc]pp)
add_library(bridge STATIC ${source_bridge})
target_link_libraries(bridge xmpp irc)

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME}
  xmpp
  irc
  bridge)

#
## Tests
#

add_executable(test src/test.cpp)
target_link_libraries(test
  xmpp
  irc
  bridge
  utils)
