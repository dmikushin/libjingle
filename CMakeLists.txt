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

project(jingle)

# Search path for CMake include files.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(NSS REQUIRED)

add_compile_options(-DPOSIX)
add_compile_options(-DWEBRTC_POSIX)
add_compile_options(-DEXPAT_RELATIVE_PATH)
add_compile_options(-DFEATURE_ENABLE_VOICEMAIL)
add_compile_options(-DGTEST_RELATIVE_PATH)
add_compile_options(-DJSONCPP_RELATIVE_PATH)
add_compile_options(-DLOGGING=1)
add_compile_options(-DSRTP_RELATIVE_PATH)

# Feature selection
add_compile_options(-DFEATURE_ENABLE_SSL)
add_compile_options(-DSSL_USE_NSS)
add_compile_options(-DSSL_USE_NSS_RNG)
add_compile_options(-DFEATURE_ENABLE_VOICEMAIL)
add_compile_options(-DFEATURE_ENABLE_PSTN)
add_compile_options(-DHAVE_SRTP)
add_compile_options(-DHAVE_WEBRTC_VIDEO)
add_compile_options(-DHAVE_WEBRTC_VOICE)
add_compile_options(-DUSE_WEBRTC_DEV_BRANCH)

add_compile_options(-DLINUX)

# Prevent GoogleTest from overriding our compiler/linker options
# when building with Visual Studio
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This adds
# the following targets: gtest, gtest_main, gmock
# and gmock_main
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/googletest" EXCLUDE_FROM_ALL)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/xmllite")

set(JSONCPP_WITH_TESTS OFF CACHE BOOL "")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/jsoncpp" EXCLUDE_FROM_ALL)
set_property(TARGET jsoncpp_lib_static PROPERTY POSITION_INDEPENDENT_CODE ON)
set(JSONCPP_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/jsoncpp/include")
set(JSONCPP_LIBRARY jsoncpp_lib_static)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/src")
add_library(${PROJECT_NAME} SHARED $<TARGET_OBJECTS:jingle_core>)
target_link_libraries(${PROJECT_NAME} ${NSS_LIBRARIES} ${JSONCPP_LIBRARY} dl rt Xext X11 Xcomposite Xrender)

#add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/examples")
#add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/tests")

