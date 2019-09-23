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

set(${PROJECT_NAME}_SRCS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/chatroommoduleimpl.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/constants.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/discoitemsquerytask.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/hangoutpubsubclient.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/iqtask.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/jid.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/moduleimpl.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/mucroomconfigtask.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/mucroomdiscoverytask.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/mucroomlookuptask.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/mucroomuniquehangoutidtask.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/pingtask.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/presenceouttask.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/presencereceivetask.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/presencestatus.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/pubsubclient.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/pubsub_task.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/pubsubtasks.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/receivetask.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/rostermoduleimpl.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/saslmechanism.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/xmppclient.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/xmppengineimpl.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/xmppengineimpl_iq.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/xmpplogintask.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/xmppstanzaparser.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/xmpptask.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/xmppauth.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/xmpppump.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/xmppsocket.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/xmppthread.cc")

add_library(${PROJECT_NAME} STATIC ${${PROJECT_NAME}_SRCS})
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

