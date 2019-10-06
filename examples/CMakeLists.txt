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

project(jingle_examples)

file(GLOB jingle_example_gtk_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/gtk/*.cc")

add_executable(jingle_example_gtk ${jingle_example_gtk_SRCS})
target_include_directories(jingle_example_gtk PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(jingle_example_gtk PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(jingle_example_gtk jingle_media jingle webrtc)

file(GLOB jingle_example_camera_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/camera/*.cc")

add_executable(jingle_example_camera ${jingle_example_camera_SRCS})
target_include_directories(jingle_example_camera PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(jingle_example_camera PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_include_directories(jingle_example_camera PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/xmllite/include")
target_include_directories(jingle_example_camera PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/xmpp/include")
target_link_libraries(jingle_example_camera xmpp xmllite jingle_p2p jingle_media jingle_sound jingle webrtc)

file(GLOB jingle_example_call_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/call/*.cc")
list(REMOVE_ITEM jingle_example_call_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/call/call_unittest.cc")
list(REMOVE_ITEM jingle_example_call_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/call/callclient_unittest.cc")

add_executable(jingle_example_call ${jingle_example_call_SRCS})
target_include_directories(jingle_example_call PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(jingle_example_call PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_include_directories(jingle_example_call PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/xmllite/include")
target_include_directories(jingle_example_call PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/xmpp/include")
target_link_libraries(jingle_example_call xmpp xmllite jingle_p2p jingle_media jingle_sound jingle webrtc)

file(GLOB jingle_example_login_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/login/*.cc")

add_executable(jingle_example_login ${jingle_example_login_SRCS})
target_include_directories(jingle_example_login PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(jingle_example_login PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_include_directories(jingle_example_login PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/xmllite/include")
target_include_directories(jingle_example_login PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/xmpp/include")
target_link_libraries(jingle_example_login jingle xmpp xmllite)

file(GLOB jingle_example_pcp_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/pcp/*.cc")

add_executable(jingle_example_pcp ${jingle_example_pcp_SRCS})
target_include_directories(jingle_example_pcp PRIVATE "${gtest_SOURCE_DIR}/include")
target_include_directories(jingle_example_pcp PRIVATE "${gmock_SOURCE_DIR}/include")
target_include_directories(jingle_example_pcp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(jingle_example_pcp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_include_directories(jingle_example_pcp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/xmllite/include")
target_include_directories(jingle_example_pcp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/xmpp/include")
target_link_libraries(jingle_example_pcp jingle_p2p jingle xmpp xmllite)

if (FALSE)
file(GLOB jingle_example_plus_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/plus/*.cc")
list(REMOVE_ITEM jingle_example_plus_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/plus/testutil/libjingleplus_unittest.cc")

add_executable(jingle_example_plus ${jingle_example_plus_SRCS})
target_include_directories(jingle_example_plus PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(jingle_example_plus PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_include_directories(jingle_example_plus PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/xmllite/include")
target_include_directories(jingle_example_plus PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/xmpp/include")
target_link_libraries(jingle_example_plus jingle_p2p jingle xmpp xmllite)
endif()

