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

project(jingle_core)

find_package(SRTP REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK2 REQUIRED gtk+-2.0)

set(${PROJECT_NAME}_SRCS
	"${CMAKE_CURRENT_SOURCE_DIR}/base/asyncfile.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/asynchttprequest.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/asyncsocket.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/asynctcpsocket.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/asyncudpsocket.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/autodetectproxy.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/bandwidthsmoother.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/base64.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/basicpacketsocketfactory.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/bytebuffer.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/checks.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/common.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/cpumonitor.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/crc32.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/diskcache.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/event.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/filelock.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/fileutils.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/firewallsocketserver.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/flags.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/helpers.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/host.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/httpbase.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/httpclient.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/httpcommon.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/httprequest.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/httpserver.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/ipaddress.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/json.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/logging.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/md5.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/messagedigest.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/messagehandler.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/messagequeue.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/multipart.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/natserver.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/natsocketfactory.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/nattypes.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/nethelpers.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/network.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/nssidentity.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/nssstreamadapter.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/optionsfile.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/pathutils.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/physicalsocketserver.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/proxydetect.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/proxyinfo.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/proxyserver.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/ratelimiter.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/ratetracker.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/sha1.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/sharedexclusivelock.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/signalthread.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/socketadapters.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/socketaddress.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/socketaddresspair.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/socketpool.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/socketstream.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/ssladapter.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/sslsocketfactory.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/sslidentity.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/sslstreamadapter.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/sslstreamadapterhelper.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/stream.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/stringencode.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/stringutils.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/systeminfo.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/task.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/taskparent.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/taskrunner.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/testclient.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/thread.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/timeutils.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/timing.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/transformadapter.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/urlencode.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/versionparsing.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/virtualsocketserver.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/worker.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/linux.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/dbus.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/libdbusglibsymboltable.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/linuxfdwalk.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/linuxwindowpicker.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/latebindingsymboltable.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/openssladapter.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/openssldigest.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/opensslidentity.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/opensslstreamadapter.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/posix.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/base/unixfilesystem.cc")

add_library(${PROJECT_NAME} OBJECT ${${PROJECT_NAME}_SRCS})
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_include_directories(${PROJECT_NAME} PRIVATE ${JSONCPP_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE "${NSS_INCLUDE_DIRS}")

set(jingle_sound_SRCS 
	"${CMAKE_CURRENT_SOURCE_DIR}/sound/nullsoundsystem.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/sound/nullsoundsystemfactory.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/sound/platformsoundsystem.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/sound/platformsoundsystemfactory.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/sound/soundsysteminterface.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/sound/soundsystemproxy.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/sound/alsasoundsystem.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/sound/alsasymboltable.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/sound/linuxsoundsystem.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/sound/pulseaudiosoundsystem.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/sound/pulseaudiosymboltable.cc")

add_library(jingle_sound STATIC ${jingle_sound_SRCS})
target_include_directories(jingle_sound PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")

set(jingle_media_SRCS
	"${CMAKE_CURRENT_SOURCE_DIR}/media/base/capturemanager.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/base/capturerenderadapter.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/base/codec.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/base/constants.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/base/cpuid.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/base/filemediaengine.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/base/hybridvideoengine.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/base/mediaengine.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/base/rtpdataengine.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/base/rtpdump.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/base/rtputils.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/base/streamparams.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/base/videoadapter.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/base/videocapturer.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/base/videocommon.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/base/videoframe.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/devices/devicemanager.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/devices/filevideocapturer.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/webrtc/webrtcpassthroughrender.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/webrtc/webrtcvideocapturer.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/webrtc/webrtcvideoengine.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/webrtc/webrtcvideoframe.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/webrtc/webrtcvoiceengine.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/devices/gtkvideorenderer.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/devices/libudevsymboltable.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/devices/linuxdeviceinfo.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/devices/linuxdevicemanager.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/media/devices/v4llookup.cc")

add_library(jingle_media STATIC ${jingle_media_SRCS})
target_include_directories(jingle_media PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_include_directories(jingle_media PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/libudev")
target_include_directories(jingle_media PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/xmllite/include")
target_include_directories(jingle_media PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/webrtc")
target_include_directories(jingle_media PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/webrtc/webrtc")
target_include_directories(jingle_media PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/libyuv/include")
target_include_directories(jingle_media PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/xmpp/include")
target_include_directories(jingle_media PRIVATE ${GTK2_INCLUDE_DIRS})
set_target_properties(jingle_media PROPERTIES COMPILE_FLAGS ${GTK2_CFLAGS_OTHER})
target_link_libraries(jingle_media ${GTK2_LIBRARIES} rt Xext X11)

set(jingle_p2p_SRCS
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/constants.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/dtlstransportchannel.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/p2ptransport.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/p2ptransportchannel.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/parsing.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/port.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/portallocator.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/portallocatorsessionproxy.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/portproxy.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/pseudotcp.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/relayport.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/relayserver.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/rawtransport.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/rawtransportchannel.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/session.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/sessiondescription.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/sessionmanager.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/sessionmessages.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/stun.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/stunport.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/stunrequest.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/stunserver.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/tcpport.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/transport.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/transportchannel.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/transportchannelproxy.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/transportdescriptionfactory.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/turnport.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/base/turnserver.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/client/basicportallocator.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/client/connectivitychecker.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/client/httpportallocator.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/p2p/client/socketmonitor.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/session/tunnel/pseudotcpchannel.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/session/tunnel/tunnelsessionclient.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/session/tunnel/securetunnelsessionclient.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/session/media/audiomonitor.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/session/media/call.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/session/media/channel.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/session/media/channelmanager.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/session/media/currentspeakermonitor.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/session/media/mediamessages.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/session/media/mediamonitor.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/session/media/mediarecorder.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/session/media/mediasession.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/session/media/mediasessionclient.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/session/media/rtcpmuxfilter.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/session/media/rtcpmuxfilter.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/session/media/soundclip.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/session/media/srtpfilter.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/session/media/ssrcmuxfilter.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/session/media/typingmonitor.cc")

add_library(jingle_p2p STATIC ${jingle_p2p_SRCS})
target_include_directories(jingle_p2p PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_include_directories(jingle_p2p PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/xmllite/include")
target_include_directories(jingle_p2p PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/xmpp/include")
target_include_directories(jingle_p2p PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include/session/tunnel")
target_include_directories(jingle_p2p PUBLIC "${gtest_SOURCE_DIR}/include")
target_include_directories(jingle_p2p PUBLIC "${gmock_SOURCE_DIR}/include")
target_include_directories(jingle_p2p PUBLIC "${SRTP_INCLUDE_DIRS}")
target_link_libraries(jingle_p2p ${SRTP_LIBRARIES})

set(jingle_peerconnection_SRCS
	"${CMAKE_CURRENT_SOURCE_DIR}/app/webrtc/audiotrack.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/app/webrtc/datachannel.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/app/webrtc/dtmfsender.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/app/webrtc/jsepicecandidate.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/app/webrtc/jsepsessiondescription.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/app/webrtc/localaudiosource.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/app/webrtc/localvideosource.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/app/webrtc/mediastream.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/app/webrtc/mediastreamhandler.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/app/webrtc/mediastreamproxy.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/app/webrtc/mediastreamsignaling.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/app/webrtc/mediastreamtrackproxy.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/app/webrtc/peerconnection.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/app/webrtc/peerconnectionfactory.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/app/webrtc/peerconnectionproxy.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/app/webrtc/portallocatorfactory.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/app/webrtc/statscollector.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/app/webrtc/videosourceproxy.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/app/webrtc/videotrack.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/app/webrtc/videotrackrenderers.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/app/webrtc/webrtcsdp.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/app/webrtc/webrtcsession.cc")

add_library(jingle_peerconnection STATIC ${jingle_peerconnection_SRCS})
target_include_directories(jingle_peerconnection PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_include_directories(jingle_peerconnection PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/xmllite/include")
target_include_directories(jingle_peerconnection PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/xmpp/include")
target_include_directories(jingle_peerconnection PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/webrtc")
target_include_directories(jingle_peerconnection PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/webrtc/webrtc")
target_include_directories(jingle_peerconnection PUBLIC "${gtest_SOURCE_DIR}/include")
target_include_directories(jingle_peerconnection PUBLIC "${gmock_SOURCE_DIR}/include")


