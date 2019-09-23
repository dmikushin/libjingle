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

project(jingle_tests)

file(GLOB jingle_base_UNIT_TESTS
	"${CMAKE_CURRENT_SOURCE_DIR}/base/*_unittest.cc")

# Remove unit test for MacOS.
list(REMOVE_ITEM jingle_base_UNIT_TESTS
	"${CMAKE_CURRENT_SOURCE_DIR}/base/macsocketserver_unittest.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/macwindowpicker_unittest.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/macutils_unittest.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/win32_unittest.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/win32toolhelp_unittest.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/winfirewall_unittest.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/win32windowpicker_unittest.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/win32socketserver_unittest.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/win32regkey_unittest.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/win32window_unittest.cc")

add_executable(jingle_base_unittest ${jingle_base_UNIT_TESTS})
target_include_directories(jingle_base_unittest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
target_include_directories(jingle_base_unittest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(jingle_base_unittest PRIVATE "${gtest_SOURCE_DIR}/include")
target_include_directories(jingle_base_unittest PRIVATE "${gmock_SOURCE_DIR}/include")
target_include_directories(jingle_base_unittest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(jingle_base_unittest jingle gtest gtest_main jingle xmllite)

file(GLOB jingle_sound_UNIT_TESTS
	"${CMAKE_CURRENT_SOURCE_DIR}/sound/*_unittest.cc")

add_executable(jingle_sound_unittest ${jingle_sound_UNIT_TESTS})
target_include_directories(jingle_sound_unittest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
target_include_directories(jingle_sound_unittest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(jingle_sound_unittest PRIVATE "${gtest_SOURCE_DIR}/include")
target_include_directories(jingle_sound_unittest PRIVATE "${gmock_SOURCE_DIR}/include")
target_include_directories(jingle_sound_unittest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(jingle_sound_unittest jingle jingle_sound gtest gtest_main jingle xmllite)

file(GLOB_RECURSE jingle_media_UNIT_TESTS
	"${CMAKE_CURRENT_SOURCE_DIR}/media/*_unittest.cc")

list(REMOVE_ITEM jingle_media_UNIT_TESTS
	"${CMAKE_CURRENT_SOURCE_DIR}/media/webrtc/webrtcvideoframe_unittest.cc")

if (FALSE)
add_executable(jingle_media_unittest ${jingle_media_UNIT_TESTS})
target_include_directories(jingle_media_unittest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
target_include_directories(jingle_media_unittest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(jingle_media_unittest PRIVATE "${gtest_SOURCE_DIR}/include")
target_include_directories(jingle_media_unittest PRIVATE "${gmock_SOURCE_DIR}/include")
target_include_directories(jingle_media_unittest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_include_directories(jingle_media_unittest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/xmllite/include")
target_include_directories(jingle_media_unittest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/webrtc")
target_include_directories(jingle_media_unittest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/webrtc/webrtc")
target_include_directories(jingle_media_unittest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/libyuv/include")
target_include_directories(jingle_media_unittest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/xmpp/include")
target_link_libraries(jingle_media_unittest jingle jingle_media gtest gtest_main jingle xmllite)
endif()

file(GLOB_RECURSE jingle_p2p_UNIT_TESTS
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/*_unittest.cc")

add_executable(jingle_p2p_unittest ${jingle_p2p_UNIT_TESTS})
target_include_directories(jingle_p2p_unittest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
target_include_directories(jingle_p2p_unittest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(jingle_p2p_unittest PRIVATE "${gtest_SOURCE_DIR}/include")
target_include_directories(jingle_p2p_unittest PRIVATE "${gmock_SOURCE_DIR}/include")
target_include_directories(jingle_p2p_unittest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_include_directories(jingle_p2p_unittest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/xmllite/include")
target_include_directories(jingle_p2p_unittest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/xmpp/include")
target_link_libraries(jingle_p2p_unittest jingle jingle_p2p gtest gtest_main jingle xmllite xmpp)

