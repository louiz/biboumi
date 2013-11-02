cmake_minimum_required(VERSION 2.6)

project(biboumi)

set(${PROJECT_NAME}_VERSION_MAJOR 0)
set(${PROJECT_NAME}_VERSION_MINOR 1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pedantic -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Og -fsanitize=address")

include_directories("src/")

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
target_link_libraries(irc network ${CRYPTO++_LIBRARIES})

#
## xmpplib
#
file(GLOB source_xmpp
  src/xmpp/*.[hc]pp)
add_library(xmpp STATIC ${source_xmpp})
target_link_libraries(xmpp network )

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME}
  xmpp
  irc)