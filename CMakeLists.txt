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

# Enable C++ 11 support
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)

# Search path for CMake include files.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(NSS REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(NSPR REQUIRED)

add_compile_options(-DPOSIX)
add_compile_options(-DWEBRTC_LINUX)
add_compile_options(-DEXPAT_RELATIVE_PATH)
add_compile_options(-DFEATURE_ENABLE_VOICEMAIL)
add_compile_options(-DGTEST_RELATIVE_PATH)
add_compile_options(-DJSONCPP_RELATIVE_PATH)
add_compile_options(-DLOGGING=1)
add_compile_options(-DSRTP_RELATIVE_PATH)

# Feature selection
add_compile_options(-DFEATURE_ENABLE_SSL)
add_compile_options(-DSSL_USE_NSS)
add_compile_options(-DHAVE_NSS_SSL_H)
add_compile_options(-DUSE_SSLSTREAM)
add_compile_options(-DNSS_SSL_RELATIVE_PATH)
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

set(${PROJECT_NAME}_SRCS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/asyncfile.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/asynchttprequest.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/asyncsocket.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/asynctcpsocket.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/asyncudpsocket.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/autodetectproxy.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/bandwidthsmoother.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/base64.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/basicpacketsocketfactory.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/bytebuffer.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/checks.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/common.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/cpumonitor.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/crc32.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/diskcache.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/event.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/filelock.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/fileutils.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/firewallsocketserver.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/flags.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/helpers.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/host.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/httpbase.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/httpclient.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/httpcommon.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/httprequest.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/httpserver.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/ipaddress.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/json.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/logging.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/md5.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/messagedigest.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/messagehandler.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/messagequeue.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/multipart.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/natserver.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/natsocketfactory.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/nattypes.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/nethelpers.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/network.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/nssidentity.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/nssstreamadapter.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/optionsfile.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/pathutils.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/physicalsocketserver.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/proxydetect.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/proxyinfo.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/proxyserver.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/ratelimiter.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/ratetracker.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/sha1.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/sharedexclusivelock.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/signalthread.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/socketadapters.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/socketaddress.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/socketaddresspair.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/socketpool.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/socketstream.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/ssladapter.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/sslsocketfactory.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/sslidentity.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/sslstreamadapter.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/sslstreamadapterhelper.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/stream.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/stringencode.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/stringutils.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/systeminfo.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/task.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/taskparent.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/taskrunner.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/testclient.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/thread.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/timeutils.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/timing.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/transformadapter.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/urlencode.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/versionparsing.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/virtualsocketserver.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/worker.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/linux.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/dbus.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/libdbusglibsymboltable.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/linuxfdwalk.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/linuxwindowpicker.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/latebindingsymboltable.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/openssladapter.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/openssldigest.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/opensslidentity.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/opensslstreamadapter.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/posix.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/base/unixfilesystem.cc")

add_library(${PROJECT_NAME} SHARED ${${PROJECT_NAME}_SRCS})
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(${PROJECT_NAME} PRIVATE ${JSONCPP_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE "${NSS_INCLUDE_DIRS}")
target_include_directories(${PROJECT_NAME} PRIVATE "${NSPR_INCLUDE_DIRS}")

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/xmllite")

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/xmpp")

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/libyuv")

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/webrtc")

set(JSONCPP_WITH_TESTS OFF CACHE BOOL "")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/jsoncpp" EXCLUDE_FROM_ALL)
set_property(TARGET jsoncpp_lib_static PROPERTY POSITION_INDEPENDENT_CODE ON)
set(JSONCPP_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/jsoncpp/include")
set(JSONCPP_LIBRARY jsoncpp_lib_static)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(${PROJECT_NAME} ${NSS_LIBRARIES} ${OPENSSL_LIBRARIES} ${NSPR_LIBRARIES} ${JSONCPP_LIBRARY} dl rt Xext X11 Xcomposite Xrender)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/tests")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/examples")

