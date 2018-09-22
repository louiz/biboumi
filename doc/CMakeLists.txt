cmake_minimum_required(VERSION 3.0)

project(biboumi_doc NONE)

include("../version.cmake")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/Modules/")
find_package(SPHINX)

if(SPHINX_FOUND)
  configure_file(conf.py.cmake conf.py)

  set(SPHINX_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/_build")

  set(HTML_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/html)
  set(MAN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/man)
  set(MAN_OUTPUT ${MAN_OUTPUT_DIR}/biboumi.1 PARENT_SCOPE)
  set(MAN_OUTPUT ${MAN_OUTPUT_DIR}/biboumi.1)
    message("${CMAKE_CURRENT_BINARY_DIR}")
  add_custom_command(OUTPUT ${HTML_OUTPUT_DIR}
      COMMAND ${SPHINX_BIN}
      -b html -q
      -c ${CMAKE_CURRENT_BINARY_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${HTML_OUTPUT_DIR})
  add_custom_command(OUTPUT ${MAN_OUTPUT}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${SPHINX_BIN}
      -b man -q
      -c ${CMAKE_CURRENT_BINARY_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${MAN_OUTPUT_DIR})
  add_custom_target(doc ALL
      DEPENDS ${HTML_OUTPUT_DIR} ${MAN_OUTPUT})
else()
  add_custom_target(doc COMMAND ${CMAKE_COMMAND} -E echo "sphinx-build is required to build the doc")
endif()
