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
file(GLOB source_libirc
  src/libirc/*.[hc]pp)
add_library(libirc STATIC ${source_libirc})
target_link_libraries(libirc network)

#
## xmpplib
#
file(GLOB source_libxmpp
  src/libxmpp/*.[hc]pp)
add_library(libxmpp STATIC ${source_libxmpp})
target_link_libraries(libxmpp network)

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME}
  libxmpp
  libirc)