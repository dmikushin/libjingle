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

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_package(SRTP REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

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
target_include_directories(jingle_media PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty")
target_include_directories(jingle_media PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/webrtc")
target_include_directories(jingle_media PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/libyuv/include")
target_include_directories(jingle_media PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/xmpp/include")
target_include_directories(jingle_media PRIVATE ${GTK3_INCLUDE_DIRS})
set_target_properties(jingle_media PROPERTIES COMPILE_FLAGS ${GTK3_CFLAGS_OTHER})
target_link_libraries(jingle_media ${GTK3_LIBRARIES} rt Xext X11)

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
target_link_libraries(jingle_p2p ${SRTP_LIBRARIES} Threads::Threads)

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
target_include_directories(jingle_peerconnection PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty")
target_include_directories(jingle_peerconnection PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/webrtc")
target_include_directories(jingle_peerconnection PUBLIC "${gtest_SOURCE_DIR}/include")
target_include_directories(jingle_peerconnection PUBLIC "${gmock_SOURCE_DIR}/include")


