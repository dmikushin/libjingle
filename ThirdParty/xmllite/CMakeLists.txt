#
# libjingle
# Copyright 2012, Google Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#  1. Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
#  3. The name of the author may not be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
# EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

cmake_minimum_required (VERSION 3.1)

project(xmllite)

find_package(EXPAT REQUIRED)

set(${PROJECT_NAME}_SRCS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/qname.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/xmlbuilder.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/xmlconstants.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/xmlelement.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/xmlnsstack.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/xmlparser.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/xmlprinter.cc")

add_library(${PROJECT_NAME} STATIC ${${PROJECT_NAME}_SRCS})
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(${PROJECT_NAME} PUBLIC "${EXPAT_INCLUDE_DIRS}")
target_link_libraries(${PROJECT_NAME} ${EXPAT_LIBRARIES})

file(GLOB ${PROJECT_NAME}_UNIT_TESTS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*_unittest.cc")

add_executable(${PROJECT_NAME}_unittest ${${PROJECT_NAME}_UNIT_TESTS})
target_include_directories(${PROJECT_NAME}_unittest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
target_include_directories(${PROJECT_NAME}_unittest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(${PROJECT_NAME}_unittest PUBLIC "${gtest_SOURCE_DIR}/include")
target_include_directories(${PROJECT_NAME}_unittest PUBLIC "${gmock_SOURCE_DIR}/include")
target_include_directories(${PROJECT_NAME}_unittest PUBLIC "${EXPAT_INCLUDE_DIRS}")
target_link_libraries(${PROJECT_NAME}_unittest ${PROJECT_NAME} gtest gtest_main jingle)

