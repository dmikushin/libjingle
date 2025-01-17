/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.h"

#include <string.h>
#include <cassert>

#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_audio.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_video.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/trace.h"

#ifdef MATLAB
#include "webrtc/modules/rtp_rtcp/test/BWEStandAlone/MatlabPlot.h"
extern MatlabEngine eng;  // Global variable defined elsewhere.
#endif

// Local for this file.
namespace {

const float kFracMs = 4.294967296E6f;

}  // namespace

#ifdef _WIN32
// Disable warning C4355: 'this' : used in base member initializer list.
#pragma warning(disable : 4355)
#endif

namespace webrtc {

static RtpData* NullObjectRtpData() {
  static NullRtpData null_rtp_data;
  return &null_rtp_data;
}

static RtpFeedback* NullObjectRtpFeedback() {
  static NullRtpFeedback null_rtp_feedback;
  return &null_rtp_feedback;
}

static RtpAudioFeedback* NullObjectRtpAudioFeedback() {
  static NullRtpAudioFeedback null_rtp_audio_feedback;
  return &null_rtp_audio_feedback;
}

RtpRtcp::Configuration::Configuration()
    : id(-1),
      audio(false),
      clock(NULL),
      default_module(NULL),
      incoming_data(NullObjectRtpData()),
      incoming_messages(NullObjectRtpFeedback()),
      outgoing_transport(NULL),
      rtcp_feedback(NULL),
      intra_frame_callback(NULL),
      bandwidth_callback(NULL),
      rtt_observer(NULL),
      audio_messages(NullObjectRtpAudioFeedback()),
      remote_bitrate_estimator(NULL),
      paced_sender(NULL) {
}

RtpRtcp* RtpRtcp::CreateRtpRtcp(const RtpRtcp::Configuration& configuration) {
  if (configuration.clock) {
    return new ModuleRtpRtcpImpl(configuration);
  } else {
    RtpRtcp::Configuration configuration_copy;
    memcpy(&configuration_copy, &configuration,
           sizeof(RtpRtcp::Configuration));
    configuration_copy.clock = Clock::GetRealTimeClock();
    ModuleRtpRtcpImpl* rtp_rtcp_instance =
        new ModuleRtpRtcpImpl(configuration_copy);
    return rtp_rtcp_instance;
  }
}

ModuleRtpRtcpImpl::ModuleRtpRtcpImpl(const Configuration& configuration)
    : rtp_payload_registry_(
          configuration.id,
          RTPPayloadStrategy::CreateStrategy(configuration.audio)),
      rtp_sender_(configuration.id,
                  configuration.audio,
                  configuration.clock,
                  configuration.outgoing_transport,
                  configuration.audio_messages,
                  configuration.paced_sender),
      rtcp_sender_(configuration.id, configuration.audio, configuration.clock,
                   this),
      rtcp_receiver_(configuration.id, configuration.clock, this),
      clock_(configuration.clock),
      rtp_telephone_event_handler_(NULL),
      id_(configuration.id),
      audio_(configuration.audio),
      collision_detected_(false),
      last_process_time_(configuration.clock->TimeInMilliseconds()),
      last_bitrate_process_time_(configuration.clock->TimeInMilliseconds()),
      last_packet_timeout_process_time_(
          configuration.clock->TimeInMilliseconds()),
      last_rtt_process_time_(configuration.clock->TimeInMilliseconds()),
      packet_overhead_(28),  // IPV4 UDP.
      critical_section_module_ptrs_(
          CriticalSectionWrapper::CreateCriticalSection()),
      critical_section_module_ptrs_feedback_(
          CriticalSectionWrapper::CreateCriticalSection()),
      default_module_(
          static_cast<ModuleRtpRtcpImpl*>(configuration.default_module)),
      dead_or_alive_active_(false),
      dead_or_alive_timeout_ms_(0),
      dead_or_alive_last_timer_(0),
      nack_method_(kNackOff),
      nack_last_time_sent_(0),
      nack_last_seq_number_sent_(0),
      simulcast_(false),
      key_frame_req_method_(kKeyFrameReqFirRtp),
      remote_bitrate_(configuration.remote_bitrate_estimator),
#ifdef MATLAB
      , plot1_(NULL),
#endif
      rtt_observer_(configuration.rtt_observer) {
  RTPReceiverStrategy* rtp_receiver_strategy;
  if (configuration.audio) {
    // If audio, we need to be able to handle telephone events too, so stash
    // away the audio receiver for those situations.
    rtp_telephone_event_handler_ =
        new RTPReceiverAudio(configuration.id, configuration.incoming_data,
                             configuration.audio_messages);
    rtp_receiver_strategy = rtp_telephone_event_handler_;
  } else {
    rtp_receiver_strategy =
        new RTPReceiverVideo(configuration.id, &rtp_payload_registry_,
                             configuration.incoming_data);
  }
  rtp_receiver_.reset(new RTPReceiver(
      configuration.id, configuration.clock, this,
      configuration.audio_messages, configuration.incoming_data,
      configuration.incoming_messages, rtp_receiver_strategy,
      &rtp_payload_registry_));


  send_video_codec_.codecType = kVideoCodecUnknown;

  if (default_module_) {
    default_module_->RegisterChildModule(this);
  }
  // TODO(pwestin) move to constructors of each rtp/rtcp sender/receiver object.
  rtcp_receiver_.RegisterRtcpObservers(configuration.intra_frame_callback,
                                       configuration.bandwidth_callback,
                                       configuration.rtcp_feedback);
  rtcp_sender_.RegisterSendTransport(configuration.outgoing_transport);

  // Make sure that RTCP objects are aware of our SSRC
  WebRtc_UWord32 SSRC = rtp_sender_.SSRC();
  rtcp_sender_.SetSSRC(SSRC);
  rtcp_receiver_.SetSSRC(SSRC);

  WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, id_, "%s created", __FUNCTION__);
}

ModuleRtpRtcpImpl::~ModuleRtpRtcpImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, id_, "%s deleted", __FUNCTION__);

  // All child modules MUST be deleted before deleting the default.
  assert(child_modules_.empty());

  // Deregister for the child modules.
  // Will go in to the default and remove it self.
  if (default_module_) {
    default_module_->DeRegisterChildModule(this);
  }
#ifdef MATLAB
  if (plot1_) {
    eng.DeletePlot(plot1_);
    plot1_ = NULL;
  }
#endif
}

void ModuleRtpRtcpImpl::RegisterChildModule(RtpRtcp* module) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "RegisterChildModule(module:0x%x)",
               module);

  CriticalSectionScoped lock(
      critical_section_module_ptrs_.get());
  CriticalSectionScoped double_lock(
      critical_section_module_ptrs_feedback_.get());

  // We use two locks for protecting child_modules_, one
  // (critical_section_module_ptrs_feedback_) for incoming
  // messages (BitrateSent) and critical_section_module_ptrs_
  // for all outgoing messages sending packets etc.
  child_modules_.push_back(static_cast<ModuleRtpRtcpImpl*>(module));
}

void ModuleRtpRtcpImpl::DeRegisterChildModule(RtpRtcp* remove_module) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "DeRegisterChildModule(module:0x%x)", remove_module);

  CriticalSectionScoped lock(
      critical_section_module_ptrs_.get());
  CriticalSectionScoped double_lock(
      critical_section_module_ptrs_feedback_.get());

  std::list<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
  while (it != child_modules_.end()) {
    RtpRtcp* module = *it;
    if (module == remove_module) {
      child_modules_.erase(it);
      return;
    }
    it++;
  }
}

// Returns the number of milliseconds until the module want a worker thread
// to call Process.
WebRtc_Word32 ModuleRtpRtcpImpl::TimeUntilNextProcess() {
    const WebRtc_Word64 now = clock_->TimeInMilliseconds();
  return kRtpRtcpMaxIdleTimeProcess - (now - last_process_time_);
}

// Process any pending tasks such as timeouts (non time critical events).
WebRtc_Word32 ModuleRtpRtcpImpl::Process() {
    const WebRtc_Word64 now = clock_->TimeInMilliseconds();
  last_process_time_ = now;

  if (now >=
      last_packet_timeout_process_time_ + kRtpRtcpPacketTimeoutProcessTimeMs) {
    rtp_receiver_->PacketTimeout();
    rtcp_receiver_.PacketTimeout();
    last_packet_timeout_process_time_ = now;
  }

  if (now >= last_bitrate_process_time_ + kRtpRtcpBitrateProcessTimeMs) {
    rtp_sender_.ProcessBitrate();
    rtp_receiver_->ProcessBitrate();
    last_bitrate_process_time_ = now;
  }

  ProcessDeadOrAliveTimer();

  const bool default_instance(child_modules_.empty() ? false : true);
  if (!default_instance) {
    if (rtcp_sender_.Sending()) {
      // Process RTT if we have received a receiver report and we haven't
      // processed RTT for at least |kRtpRtcpRttProcessTimeMs| milliseconds.
      if (rtcp_receiver_.LastReceivedReceiverReport() >
          last_rtt_process_time_ && now >= last_rtt_process_time_ +
          kRtpRtcpRttProcessTimeMs) {
        last_rtt_process_time_ = now;
        std::vector<RTCPReportBlock> receive_blocks;
        rtcp_receiver_.StatisticsReceived(&receive_blocks);
        uint16_t max_rtt = 0;
        for (std::vector<RTCPReportBlock>::iterator it = receive_blocks.begin();
             it != receive_blocks.end(); ++it) {
          uint16_t rtt = 0;
          rtcp_receiver_.RTT(it->remoteSSRC, &rtt, NULL, NULL, NULL);
          max_rtt = (rtt > max_rtt) ? rtt : max_rtt;
        }
        // Report the rtt.
        if (rtt_observer_ && max_rtt != 0)
          rtt_observer_->OnRttUpdate(max_rtt);
      }

      // Verify receiver reports are delivered and the reported sequence number
      // is increasing.
      int64_t rtcp_interval = RtcpReportInterval();
      if (rtcp_receiver_.RtcpRrTimeout(rtcp_interval)) {
        LOG_F(LS_WARNING) << "Timeout: No RTCP RR received.";
      } else if (rtcp_receiver_.RtcpRrSequenceNumberTimeout(rtcp_interval)) {
        LOG_F(LS_WARNING) <<
            "Timeout: No increase in RTCP RR extended highest sequence number.";
      }

      if (remote_bitrate_ && TMMBR()) {
        unsigned int target_bitrate = 0;
        std::vector<unsigned int> ssrcs;
        if (remote_bitrate_->LatestEstimate(&ssrcs, &target_bitrate)) {
          if (!ssrcs.empty()) {
            target_bitrate = target_bitrate / ssrcs.size();
          }
          rtcp_sender_.SetTargetBitrate(target_bitrate);
        }
      }
    }
    if (rtcp_sender_.TimeToSendRTCPReport())
      rtcp_sender_.SendRTCP(kRtcpReport);
  }

  if (UpdateRTCPReceiveInformationTimers()) {
    // A receiver has timed out
    rtcp_receiver_.UpdateTMMBR();
  }
  return 0;
}

void ModuleRtpRtcpImpl::ProcessDeadOrAliveTimer() {
  if (dead_or_alive_active_) {
    const WebRtc_Word64 now = clock_->TimeInMilliseconds();
    if (now > dead_or_alive_timeout_ms_ + dead_or_alive_last_timer_) {
      // RTCP is alive if we have received a report the last 12 seconds.
      dead_or_alive_last_timer_ += dead_or_alive_timeout_ms_;

      bool RTCPalive = false;
      if (rtcp_receiver_.LastReceived() + 12000 > now) {
        RTCPalive = true;
      }
      rtp_receiver_->ProcessDeadOrAlive(RTCPalive, now);
    }
  }
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetPeriodicDeadOrAliveStatus(
    const bool enable,
    const WebRtc_UWord8 sample_time_seconds) {
  if (enable) {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 id_,
                 "SetPeriodicDeadOrAliveStatus(enable, %d)",
                 sample_time_seconds);
  } else {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 id_,
                 "SetPeriodicDeadOrAliveStatus(disable)");
  }
  if (sample_time_seconds == 0) {
    return -1;
  }
  dead_or_alive_active_ = enable;
  dead_or_alive_timeout_ms_ = sample_time_seconds * 1000;
  // Trigger the first after one period.
  dead_or_alive_last_timer_ = clock_->TimeInMilliseconds();
  return 0;
}

WebRtc_Word32 ModuleRtpRtcpImpl::PeriodicDeadOrAliveStatus(
    bool& enable,
    WebRtc_UWord8& sample_time_seconds) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "PeriodicDeadOrAliveStatus()");

  enable = dead_or_alive_active_;
  sample_time_seconds =
      static_cast<WebRtc_UWord8>(dead_or_alive_timeout_ms_ / 1000);
  return 0;
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetPacketTimeout(
    const WebRtc_UWord32 rtp_timeout_ms,
    const WebRtc_UWord32 rtcp_timeout_ms) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetPacketTimeout(%u,%u)",
               rtp_timeout_ms,
               rtcp_timeout_ms);

  if (rtp_receiver_->SetPacketTimeout(rtp_timeout_ms) == 0) {
    return rtcp_receiver_.SetPacketTimeout(rtcp_timeout_ms);
  }
  return -1;
}

WebRtc_Word32 ModuleRtpRtcpImpl::RegisterReceivePayload(
    const CodecInst& voice_codec) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "RegisterReceivePayload(voice_codec)");

  return rtp_receiver_->RegisterReceivePayload(
      voice_codec.plname,
      voice_codec.pltype,
      voice_codec.plfreq,
      voice_codec.channels,
      (voice_codec.rate < 0) ? 0 : voice_codec.rate);
}

WebRtc_Word32 ModuleRtpRtcpImpl::RegisterReceivePayload(
    const VideoCodec& video_codec) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "RegisterReceivePayload(video_codec)");

  return rtp_receiver_->RegisterReceivePayload(video_codec.plName,
                                               video_codec.plType,
                                               90000,
                                               0,
                                               video_codec.maxBitrate);
}

WebRtc_Word32 ModuleRtpRtcpImpl::ReceivePayloadType(
    const CodecInst& voice_codec,
  WebRtc_Word8* pl_type) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "ReceivePayloadType(voice_codec)");

  return rtp_receiver_->ReceivePayloadType(
           voice_codec.plname,
           voice_codec.plfreq,
           voice_codec.channels,
           (voice_codec.rate < 0) ? 0 : voice_codec.rate,
           pl_type);
}

WebRtc_Word32 ModuleRtpRtcpImpl::ReceivePayloadType(
    const VideoCodec& video_codec,
  WebRtc_Word8* pl_type) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "ReceivePayloadType(video_codec)");

  return rtp_receiver_->ReceivePayloadType(video_codec.plName,
                                           90000,
                                           0,
                                           video_codec.maxBitrate,
                                           pl_type);
}

WebRtc_Word32 ModuleRtpRtcpImpl::DeRegisterReceivePayload(
    const WebRtc_Word8 payload_type) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "DeRegisterReceivePayload(%d)",
               payload_type);

  return rtp_receiver_->DeRegisterReceivePayload(payload_type);
}

// Get the currently configured SSRC filter.
WebRtc_Word32 ModuleRtpRtcpImpl::SSRCFilter(
    WebRtc_UWord32& allowed_ssrc) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SSRCFilter()");

  return rtp_receiver_->SSRCFilter(allowed_ssrc);
}

// Set a SSRC to be used as a filter for incoming RTP streams.
WebRtc_Word32 ModuleRtpRtcpImpl::SetSSRCFilter(
    const bool enable,
    const WebRtc_UWord32 allowed_ssrc) {
  if (enable) {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 id_,
                 "SetSSRCFilter(enable, 0x%x)",
                 allowed_ssrc);
  } else {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 id_,
                 "SetSSRCFilter(disable)");
  }

  return rtp_receiver_->SetSSRCFilter(enable, allowed_ssrc);
}

// Get last received remote timestamp.
WebRtc_UWord32 ModuleRtpRtcpImpl::RemoteTimestamp() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "RemoteTimestamp()");

  return rtp_receiver_->TimeStamp();
}

int64_t ModuleRtpRtcpImpl::LocalTimeOfRemoteTimeStamp() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
               "LocalTimeOfRemoteTimeStamp()");

  return rtp_receiver_->LastReceivedTimeMs();
}

// Get the current estimated remote timestamp.
WebRtc_Word32 ModuleRtpRtcpImpl::EstimatedRemoteTimeStamp(
    WebRtc_UWord32& timestamp) const {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "EstimatedRemoteTimeStamp()");

  return rtp_receiver_->EstimatedRemoteTimeStamp(timestamp);
}

// Get incoming SSRC.
WebRtc_UWord32 ModuleRtpRtcpImpl::RemoteSSRC() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "RemoteSSRC()");

  return rtp_receiver_->SSRC();
}

// Get remote CSRC
WebRtc_Word32 ModuleRtpRtcpImpl::RemoteCSRCs(
    WebRtc_UWord32 arr_of_csrc[kRtpCsrcSize]) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "RemoteCSRCs()");

  return rtp_receiver_->CSRCs(arr_of_csrc);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetRTXSendStatus(
    const bool enable,
    const bool set_ssrc,
    const WebRtc_UWord32 ssrc) {
  rtp_sender_.SetRTXStatus(enable, set_ssrc, ssrc);
  return 0;
}

WebRtc_Word32 ModuleRtpRtcpImpl::RTXSendStatus(bool* enable,
                                               WebRtc_UWord32* ssrc) const {
  rtp_sender_.RTXStatus(enable, ssrc);
  return 0;
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetRTXReceiveStatus(
    const bool enable,
    const WebRtc_UWord32 ssrc) {
  rtp_receiver_->SetRTXStatus(enable, ssrc);
  return 0;
}

WebRtc_Word32 ModuleRtpRtcpImpl::RTXReceiveStatus(bool* enable,
                                                  WebRtc_UWord32* ssrc) const {
  rtp_receiver_->RTXStatus(enable, ssrc);
  return 0;
}

// Called by the network module when we receive a packet.
WebRtc_Word32 ModuleRtpRtcpImpl::IncomingPacket(
    const WebRtc_UWord8* incoming_packet,
    const WebRtc_UWord16 incoming_packet_length) {
  WEBRTC_TRACE(kTraceStream,
               kTraceRtpRtcp,
               id_,
               "IncomingPacket(packet_length:%u)",
               incoming_packet_length);
  // Minimum RTP is 12 bytes.
  // Minimum RTCP is 8 bytes (RTCP BYE).
  if (incoming_packet_length < 8 || incoming_packet == NULL) {
    WEBRTC_TRACE(kTraceDebug,
                 kTraceRtpRtcp,
                 id_,
                 "IncomingPacket invalid buffer or length");
    return -1;
  }
  // Check RTP version.
  const WebRtc_UWord8 version = incoming_packet[0] >> 6;
  if (version != 2) {
    WEBRTC_TRACE(kTraceDebug,
                 kTraceRtpRtcp,
                 id_,
                 "IncomingPacket invalid RTP version");
    return -1;
  }

  ModuleRTPUtility::RTPHeaderParser rtp_parser(incoming_packet,
                                               incoming_packet_length);

  if (rtp_parser.RTCP()) {
    // Allow receive of non-compound RTCP packets.
    RTCPUtility::RTCPParserV2 rtcp_parser(incoming_packet,
                                          incoming_packet_length,
                                          true);

    const bool valid_rtcpheader = rtcp_parser.IsValid();
    if (!valid_rtcpheader) {
      WEBRTC_TRACE(kTraceDebug,
                   kTraceRtpRtcp,
                   id_,
                   "IncomingPacket invalid RTCP packet");
      return -1;
    }
    RTCPHelp::RTCPPacketInformation rtcp_packet_information;
    WebRtc_Word32 ret_val = rtcp_receiver_.IncomingRTCPPacket(
        rtcp_packet_information, &rtcp_parser);
    if (ret_val == 0) {
      rtcp_receiver_.TriggerCallbacksFromRTCPPacket(rtcp_packet_information);
    }
    return ret_val;

  } else {
    WebRtcRTPHeader rtp_header;
    memset(&rtp_header, 0, sizeof(rtp_header));

    RtpHeaderExtensionMap map;
    rtp_receiver_->GetHeaderExtensionMapCopy(&map);

    const bool valid_rtpheader = rtp_parser.Parse(rtp_header, &map);
    if (!valid_rtpheader) {
      WEBRTC_TRACE(kTraceDebug,
                   kTraceRtpRtcp,
                   id_,
                   "IncomingPacket invalid RTP header");
      return -1;
    }
    return rtp_receiver_->IncomingRTPPacket(&rtp_header,
                                            incoming_packet,
                                            incoming_packet_length);
  }
}

WebRtc_Word32 ModuleRtpRtcpImpl::RegisterSendPayload(
    const CodecInst& voice_codec) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "RegisterSendPayload(pl_name:%s pl_type:%d frequency:%u)",
               voice_codec.plname,
               voice_codec.pltype,
               voice_codec.plfreq);

  return rtp_sender_.RegisterPayload(
           voice_codec.plname,
           voice_codec.pltype,
           voice_codec.plfreq,
           voice_codec.channels,
           (voice_codec.rate < 0) ? 0 : voice_codec.rate);
}

WebRtc_Word32 ModuleRtpRtcpImpl::RegisterSendPayload(
    const VideoCodec& video_codec) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "RegisterSendPayload(pl_name:%s pl_type:%d)",
               video_codec.plName,
               video_codec.plType);

  send_video_codec_ = video_codec;
  simulcast_ = (video_codec.numberOfSimulcastStreams > 1) ? true : false;
  return rtp_sender_.RegisterPayload(video_codec.plName,
                                     video_codec.plType,
                                     90000,
                                     0,
                                     video_codec.maxBitrate);
}

WebRtc_Word32 ModuleRtpRtcpImpl::DeRegisterSendPayload(
    const WebRtc_Word8 payload_type) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "DeRegisterSendPayload(%d)", payload_type);

  return rtp_sender_.DeRegisterSendPayload(payload_type);
}

WebRtc_Word8 ModuleRtpRtcpImpl::SendPayloadType() const {
  return rtp_sender_.SendPayloadType();
}

WebRtc_UWord32 ModuleRtpRtcpImpl::StartTimestamp() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "StartTimestamp()");

  return rtp_sender_.StartTimestamp();
}

// Configure start timestamp, default is a random number.
WebRtc_Word32 ModuleRtpRtcpImpl::SetStartTimestamp(
    const WebRtc_UWord32 timestamp) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetStartTimestamp(%d)",
               timestamp);
  rtcp_sender_.SetStartTimestamp(timestamp);
  rtp_sender_.SetStartTimestamp(timestamp, true);
  return 0;  // TODO(pwestin): change to void.
}

WebRtc_UWord16 ModuleRtpRtcpImpl::SequenceNumber() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SequenceNumber()");

  return rtp_sender_.SequenceNumber();
}

// Set SequenceNumber, default is a random number.
WebRtc_Word32 ModuleRtpRtcpImpl::SetSequenceNumber(
    const WebRtc_UWord16 seq_num) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetSequenceNumber(%d)",
               seq_num);

  rtp_sender_.SetSequenceNumber(seq_num);
  return 0;  // TODO(pwestin): change to void.
}

WebRtc_UWord32 ModuleRtpRtcpImpl::SSRC() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SSRC()");

  return rtp_sender_.SSRC();
}

// Configure SSRC, default is a random number.
WebRtc_Word32 ModuleRtpRtcpImpl::SetSSRC(const WebRtc_UWord32 ssrc) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SetSSRC(%d)", ssrc);

  rtp_sender_.SetSSRC(ssrc);
  rtcp_receiver_.SetSSRC(ssrc);
  rtcp_sender_.SetSSRC(ssrc);
  return 0;  // TODO(pwestin): change to void.
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetCSRCStatus(const bool include) {
  rtcp_sender_.SetCSRCStatus(include);
  rtp_sender_.SetCSRCStatus(include);
  return 0;  // TODO(pwestin): change to void.
}

WebRtc_Word32 ModuleRtpRtcpImpl::CSRCs(
  WebRtc_UWord32 arr_of_csrc[kRtpCsrcSize]) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "CSRCs()");

  return rtp_sender_.CSRCs(arr_of_csrc);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetCSRCs(
    const WebRtc_UWord32 arr_of_csrc[kRtpCsrcSize],
    const WebRtc_UWord8 arr_length) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetCSRCs(arr_length:%d)",
               arr_length);

  const bool default_instance(child_modules_.empty() ? false : true);

  if (default_instance) {
    // For default we need to update all child modules too.
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());

    std::list<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
    while (it != child_modules_.end()) {
      RtpRtcp* module = *it;
      if (module) {
        module->SetCSRCs(arr_of_csrc, arr_length);
      }
      it++;
    }
  } else {
    for (int i = 0; i < arr_length; ++i) {
      WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "\tidx:%d CSRC:%u", i,
                   arr_of_csrc[i]);
    }
    rtcp_sender_.SetCSRCs(arr_of_csrc, arr_length);
    rtp_sender_.SetCSRCs(arr_of_csrc, arr_length);
  }
  return 0;  // TODO(pwestin): change to void.
}

WebRtc_UWord32 ModuleRtpRtcpImpl::PacketCountSent() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "PacketCountSent()");

  return rtp_sender_.Packets();
}

WebRtc_UWord32 ModuleRtpRtcpImpl::ByteCountSent() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "ByteCountSent()");

  return rtp_sender_.Bytes();
}

int ModuleRtpRtcpImpl::CurrentSendFrequencyHz() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
               "CurrentSendFrequencyHz()");

  return rtp_sender_.SendPayloadFrequency();
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetSendingStatus(const bool sending) {
  if (sending) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
                 "SetSendingStatus(sending)");
  } else {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
                 "SetSendingStatus(stopped)");
  }
  if (rtcp_sender_.Sending() != sending) {
    // Sends RTCP BYE when going from true to false
    if (rtcp_sender_.SetSendingStatus(sending) != 0) {
      WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, id_,
                   "Failed to send RTCP BYE");
    }

    collision_detected_ = false;

    // Generate a new time_stamp if true and not configured via API
    // Generate a new SSRC for the next "call" if false
    rtp_sender_.SetSendingStatus(sending);
    if (sending) {
      // Make sure the RTCP sender has the same timestamp offset.
      rtcp_sender_.SetStartTimestamp(rtp_sender_.StartTimestamp());
    }

    // Make sure that RTCP objects are aware of our SSRC (it could have changed
    // Due to collision)
    WebRtc_UWord32 SSRC = rtp_sender_.SSRC();
    rtcp_receiver_.SetSSRC(SSRC);
    rtcp_sender_.SetSSRC(SSRC);
    return 0;
  }
  return 0;
}

bool ModuleRtpRtcpImpl::Sending() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "Sending()");

  return rtcp_sender_.Sending();
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetSendingMediaStatus(const bool sending) {
  if (sending) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
                 "SetSendingMediaStatus(sending)");
  } else {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
                 "SetSendingMediaStatus(stopped)");
  }
  rtp_sender_.SetSendingMediaStatus(sending);
  return 0;
}

bool ModuleRtpRtcpImpl::SendingMedia() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "Sending()");

  const bool have_child_modules(child_modules_.empty() ? false : true);
  if (!have_child_modules) {
    return rtp_sender_.SendingMedia();
  }

  CriticalSectionScoped lock(critical_section_module_ptrs_.get());
  std::list<ModuleRtpRtcpImpl*>::const_iterator it = child_modules_.begin();
  while (it != child_modules_.end()) {
    RTPSender& rtp_sender = (*it)->rtp_sender_;
    if (rtp_sender.SendingMedia()) {
      return true;
    }
    it++;
  }
  return false;
}

WebRtc_Word32 ModuleRtpRtcpImpl::SendOutgoingData(
    FrameType frame_type,
    WebRtc_Word8 payload_type,
    WebRtc_UWord32 time_stamp,
    int64_t capture_time_ms,
    const WebRtc_UWord8* payload_data,
    WebRtc_UWord32 payload_size,
    const RTPFragmentationHeader* fragmentation,
    const RTPVideoHeader* rtp_video_hdr) {
  WEBRTC_TRACE(
    kTraceStream,
    kTraceRtpRtcp,
    id_,
    "SendOutgoingData(frame_type:%d payload_type:%d time_stamp:%u size:%u)",
    frame_type, payload_type, time_stamp, payload_size);

  rtcp_sender_.SetLastRtpTime(time_stamp, capture_time_ms);

  const bool have_child_modules(child_modules_.empty() ? false : true);
  if (!have_child_modules) {
    // Don't send RTCP from default module.
    if (rtcp_sender_.TimeToSendRTCPReport(kVideoFrameKey == frame_type)) {
      rtcp_sender_.SendRTCP(kRtcpReport);
    }
    return rtp_sender_.SendOutgoingData(frame_type,
                                        payload_type,
                                        time_stamp,
                                        capture_time_ms,
                                        payload_data,
                                        payload_size,
                                        fragmentation,
                                        NULL,
                                        &(rtp_video_hdr->codecHeader));
  }
  WebRtc_Word32 ret_val = -1;
  if (simulcast_) {
    if (rtp_video_hdr == NULL) {
      return -1;
    }
    int idx = 0;
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());
    std::list<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
    for (; idx < rtp_video_hdr->simulcastIdx; ++it) {
      if (it == child_modules_.end()) {
        return -1;
      }
      if ((*it)->SendingMedia()) {
        ++idx;
      }
    }
    for (; it != child_modules_.end(); ++it) {
      if ((*it)->SendingMedia()) {
        break;
      }
      ++idx;
    }
    if (it == child_modules_.end()) {
      return -1;
    }
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 id_,
                 "SendOutgoingData(SimulcastIdx:%u size:%u, ssrc:0x%x)",
                 idx, payload_size, (*it)->rtp_sender_.SSRC());
    return (*it)->SendOutgoingData(frame_type,
                                   payload_type,
                                   time_stamp,
                                   capture_time_ms,
                                   payload_data,
                                   payload_size,
                                   fragmentation,
                                   rtp_video_hdr);
  } else {
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());

    std::list<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
    if (it != child_modules_.end()) {
      ret_val = (*it)->SendOutgoingData(frame_type,
                                        payload_type,
                                        time_stamp,
                                        capture_time_ms,
                                        payload_data,
                                        payload_size,
                                        fragmentation,
                                        rtp_video_hdr);

      it++;
    }

    // Send to all remaining "child" modules
    while (it != child_modules_.end()) {
      ret_val = (*it)->SendOutgoingData(frame_type,
                                        payload_type,
                                        time_stamp,
                                        capture_time_ms,
                                        payload_data,
                                        payload_size,
                                        fragmentation,
                                        rtp_video_hdr);

      it++;
    }
  }
  return ret_val;
}

void ModuleRtpRtcpImpl::TimeToSendPacket(uint32_t ssrc,
                                         uint16_t sequence_number,
                                         int64_t capture_time_ms) {
  WEBRTC_TRACE(
    kTraceStream,
    kTraceRtpRtcp,
    id_,
    "TimeToSendPacket(ssrc:0x%x sequence_number:%u capture_time_ms:%ll)",
    ssrc, sequence_number, capture_time_ms);

  if (simulcast_) {
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());
    std::list<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
    while (it != child_modules_.end()) {
      if ((*it)->SendingMedia() && ssrc == (*it)->rtp_sender_.SSRC()) {
        (*it)->rtp_sender_.TimeToSendPacket(sequence_number, capture_time_ms);
        return;
      }
      it++;
    }
  } else {
    bool have_child_modules(child_modules_.empty() ? false : true);
    if (!have_child_modules) {
      // Don't send from default module.
      if (SendingMedia() && ssrc == rtp_sender_.SSRC()) {
        rtp_sender_.TimeToSendPacket(sequence_number, capture_time_ms);
      }
    }
  }
}

WebRtc_UWord16 ModuleRtpRtcpImpl::MaxPayloadLength() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "MaxPayloadLength()");

  return rtp_sender_.MaxPayloadLength();
}

WebRtc_UWord16 ModuleRtpRtcpImpl::MaxDataPayloadLength() const {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "MaxDataPayloadLength()");

  // Assuming IP/UDP.
  WebRtc_UWord16 min_data_payload_length = IP_PACKET_SIZE - 28;

  const bool default_instance(child_modules_.empty() ? false : true);
  if (default_instance) {
    // For default we need to update all child modules too.
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());
    std::list<ModuleRtpRtcpImpl*>::const_iterator it =
      child_modules_.begin();
    while (it != child_modules_.end()) {
      RtpRtcp* module = *it;
      if (module) {
        WebRtc_UWord16 data_payload_length =
          module->MaxDataPayloadLength();
        if (data_payload_length < min_data_payload_length) {
          min_data_payload_length = data_payload_length;
        }
      }
      it++;
    }
  }

  WebRtc_UWord16 data_payload_length = rtp_sender_.MaxDataPayloadLength();
  if (data_payload_length < min_data_payload_length) {
    min_data_payload_length = data_payload_length;
  }
  return min_data_payload_length;
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetTransportOverhead(
    const bool tcp,
    const bool ipv6,
    const WebRtc_UWord8 authentication_overhead) {
  WEBRTC_TRACE(
    kTraceModuleCall,
    kTraceRtpRtcp,
    id_,
    "SetTransportOverhead(TCP:%d, IPV6:%d authentication_overhead:%u)",
    tcp, ipv6, authentication_overhead);

  WebRtc_UWord16 packet_overhead = 0;
  if (ipv6) {
    packet_overhead = 40;
  } else {
    packet_overhead = 20;
  }
  if (tcp) {
    // TCP.
    packet_overhead += 20;
  } else {
    // UDP.
    packet_overhead += 8;
  }
  packet_overhead += authentication_overhead;

  if (packet_overhead == packet_overhead_) {
    // Ok same as before.
    return 0;
  }
  // Calc diff.
  WebRtc_Word16 packet_over_head_diff = packet_overhead - packet_overhead_;

  // Store new.
  packet_overhead_ = packet_overhead;

  WebRtc_UWord16 length =
      rtp_sender_.MaxPayloadLength() - packet_over_head_diff;
  return rtp_sender_.SetMaxPayloadLength(length, packet_overhead_);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetMaxTransferUnit(const WebRtc_UWord16 mtu) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SetMaxTransferUnit(%u)",
               mtu);

  if (mtu > IP_PACKET_SIZE) {
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, id_,
                 "Invalid in argument to SetMaxTransferUnit(%u)", mtu);
    return -1;
  }
  return rtp_sender_.SetMaxPayloadLength(mtu - packet_overhead_,
                                         packet_overhead_);
}

RTCPMethod ModuleRtpRtcpImpl::RTCP() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "RTCP()");

  if (rtcp_sender_.Status() != kRtcpOff) {
    return rtcp_receiver_.Status();
  }
  return kRtcpOff;
}

// Configure RTCP status i.e on/off.
WebRtc_Word32 ModuleRtpRtcpImpl::SetRTCPStatus(const RTCPMethod method) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SetRTCPStatus(%d)",
               method);

  if (rtcp_sender_.SetRTCPStatus(method) == 0) {
    return rtcp_receiver_.SetRTCPStatus(method);
  }
  return -1;
}

// Only for internal test.
WebRtc_UWord32 ModuleRtpRtcpImpl::LastSendReport(
    WebRtc_UWord32& last_rtcptime) {
  return rtcp_sender_.LastSendReport(last_rtcptime);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetCNAME(const char c_name[RTCP_CNAME_SIZE]) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SetCNAME(%s)", c_name);
  return rtcp_sender_.SetCNAME(c_name);
}

WebRtc_Word32 ModuleRtpRtcpImpl::CNAME(char c_name[RTCP_CNAME_SIZE]) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "CNAME()");
  return rtcp_sender_.CNAME(c_name);
}

WebRtc_Word32 ModuleRtpRtcpImpl::AddMixedCNAME(
  const WebRtc_UWord32 ssrc,
  const char c_name[RTCP_CNAME_SIZE]) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
               "AddMixedCNAME(SSRC:%u)", ssrc);

  return rtcp_sender_.AddMixedCNAME(ssrc, c_name);
}

WebRtc_Word32 ModuleRtpRtcpImpl::RemoveMixedCNAME(const WebRtc_UWord32 ssrc) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
               "RemoveMixedCNAME(SSRC:%u)", ssrc);
  return rtcp_sender_.RemoveMixedCNAME(ssrc);
}

WebRtc_Word32 ModuleRtpRtcpImpl::RemoteCNAME(
    const WebRtc_UWord32 remote_ssrc,
    char c_name[RTCP_CNAME_SIZE]) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
               "RemoteCNAME(SSRC:%u)", remote_ssrc);

  return rtcp_receiver_.CNAME(remote_ssrc, c_name);
}

WebRtc_UWord16 ModuleRtpRtcpImpl::RemoteSequenceNumber() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "RemoteSequenceNumber()");

  return rtp_receiver_->SequenceNumber();
}

WebRtc_Word32 ModuleRtpRtcpImpl::RemoteNTP(
    WebRtc_UWord32* received_ntpsecs,
    WebRtc_UWord32* received_ntpfrac,
    WebRtc_UWord32* rtcp_arrival_time_secs,
    WebRtc_UWord32* rtcp_arrival_time_frac,
    WebRtc_UWord32* rtcp_timestamp) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "RemoteNTP()");

  return rtcp_receiver_.NTP(received_ntpsecs,
                            received_ntpfrac,
                            rtcp_arrival_time_secs,
                            rtcp_arrival_time_frac,
                            rtcp_timestamp);
}

// Get RoundTripTime.
WebRtc_Word32 ModuleRtpRtcpImpl::RTT(const WebRtc_UWord32 remote_ssrc,
                                     WebRtc_UWord16* rtt,
                                     WebRtc_UWord16* avg_rtt,
                                     WebRtc_UWord16* min_rtt,
                                     WebRtc_UWord16* max_rtt) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "RTT()");

  return rtcp_receiver_.RTT(remote_ssrc, rtt, avg_rtt, min_rtt, max_rtt);
}

// Reset RoundTripTime statistics.
WebRtc_Word32 ModuleRtpRtcpImpl::ResetRTT(const WebRtc_UWord32 remote_ssrc) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "ResetRTT(SSRC:%u)",
               remote_ssrc);

  return rtcp_receiver_.ResetRTT(remote_ssrc);
}

void ModuleRtpRtcpImpl:: SetRtt(uint32_t rtt) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SetRtt(rtt: %u)", rtt);
  rtcp_receiver_.SetRTT(static_cast<WebRtc_UWord16>(rtt));
}

// Reset RTP statistics.
WebRtc_Word32 ModuleRtpRtcpImpl::ResetStatisticsRTP() {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "ResetStatisticsRTP()");

  return rtp_receiver_->ResetStatistics();
}

// Reset RTP data counters for the receiving side.
WebRtc_Word32 ModuleRtpRtcpImpl::ResetReceiveDataCountersRTP() {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
               "ResetReceiveDataCountersRTP()");

  return rtp_receiver_->ResetDataCounters();
}

// Reset RTP data counters for the sending side.
WebRtc_Word32 ModuleRtpRtcpImpl::ResetSendDataCountersRTP() {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
               "ResetSendDataCountersRTP()");

  rtp_sender_.ResetDataCounters();
  return 0;  // TODO(pwestin): change to void.
}

// Force a send of an RTCP packet.
// Normal SR and RR are triggered via the process function.
WebRtc_Word32 ModuleRtpRtcpImpl::SendRTCP(WebRtc_UWord32 rtcp_packet_type) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SendRTCP(0x%x)",
               rtcp_packet_type);

  return  rtcp_sender_.SendRTCP(rtcp_packet_type);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetRTCPApplicationSpecificData(
    const WebRtc_UWord8 sub_type,
    const WebRtc_UWord32 name,
    const WebRtc_UWord8* data,
    const WebRtc_UWord16 length) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
               "SetRTCPApplicationSpecificData(sub_type:%d name:0x%x)",
               sub_type, name);

  return  rtcp_sender_.SetApplicationSpecificData(sub_type, name, data, length);
}

// (XR) VOIP metric.
WebRtc_Word32 ModuleRtpRtcpImpl::SetRTCPVoIPMetrics(
  const RTCPVoIPMetric* voip_metric) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SetRTCPVoIPMetrics()");

  return  rtcp_sender_.SetRTCPVoIPMetrics(voip_metric);
}

// Our locally created statistics of the received RTP stream.
WebRtc_Word32 ModuleRtpRtcpImpl::StatisticsRTP(
    WebRtc_UWord8*  fraction_lost,
    WebRtc_UWord32* cum_lost,
    WebRtc_UWord32* ext_max,
    WebRtc_UWord32* jitter,
    WebRtc_UWord32* max_jitter) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "StatisticsRTP()");

  WebRtc_UWord32 jitter_transmission_time_offset = 0;

  WebRtc_Word32 ret_val = rtp_receiver_->Statistics(
      fraction_lost, cum_lost, ext_max, jitter, max_jitter,
      &jitter_transmission_time_offset, (rtcp_sender_.Status() == kRtcpOff));
  if (ret_val == -1) {
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, id_,
                 "StatisticsRTP() no statistics available");
  }
  return ret_val;
}

WebRtc_Word32 ModuleRtpRtcpImpl::DataCountersRTP(
    WebRtc_UWord32* bytes_sent,
    WebRtc_UWord32* packets_sent,
    WebRtc_UWord32* bytes_received,
    WebRtc_UWord32* packets_received) const {
  WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, id_, "DataCountersRTP()");

  if (bytes_sent) {
    *bytes_sent = rtp_sender_.Bytes();
  }
  if (packets_sent) {
    *packets_sent = rtp_sender_.Packets();
  }
  return rtp_receiver_->DataCounters(bytes_received, packets_received);
}

WebRtc_Word32 ModuleRtpRtcpImpl::ReportBlockStatistics(
    WebRtc_UWord8* fraction_lost,
    WebRtc_UWord32* cum_lost,
    WebRtc_UWord32* ext_max,
    WebRtc_UWord32* jitter,
    WebRtc_UWord32* jitter_transmission_time_offset) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "ReportBlockStatistics()");
  WebRtc_Word32 missing = 0;
  WebRtc_Word32 ret = rtp_receiver_->Statistics(fraction_lost,
                                                cum_lost,
                                                ext_max,
                                                jitter,
                                                NULL,
                                                jitter_transmission_time_offset,
                                                &missing,
                                                true);

#ifdef MATLAB
  if (plot1_ == NULL) {
    plot1_ = eng.NewPlot(new MatlabPlot());
    plot1_->AddTimeLine(30, "b", "lost", clock_->TimeInMilliseconds());
  }
  plot1_->Append("lost", missing);
  plot1_->Plot();
#endif

  return ret;
}

WebRtc_Word32 ModuleRtpRtcpImpl::RemoteRTCPStat(RTCPSenderInfo* sender_info) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "RemoteRTCPStat()");

  return rtcp_receiver_.SenderInfoReceived(sender_info);
}

// Received RTCP report.
WebRtc_Word32 ModuleRtpRtcpImpl::RemoteRTCPStat(
    std::vector<RTCPReportBlock>* receive_blocks) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "RemoteRTCPStat()");

  return rtcp_receiver_.StatisticsReceived(receive_blocks);
}

WebRtc_Word32 ModuleRtpRtcpImpl::AddRTCPReportBlock(
    const WebRtc_UWord32 ssrc,
    const RTCPReportBlock* report_block) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "AddRTCPReportBlock()");

  return rtcp_sender_.AddReportBlock(ssrc, report_block);
}

WebRtc_Word32 ModuleRtpRtcpImpl::RemoveRTCPReportBlock(
  const WebRtc_UWord32 ssrc) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "RemoveRTCPReportBlock()");

  return rtcp_sender_.RemoveReportBlock(ssrc);
}

// (REMB) Receiver Estimated Max Bitrate.
bool ModuleRtpRtcpImpl::REMB() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "REMB()");

  return rtcp_sender_.REMB();
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetREMBStatus(const bool enable) {
  if (enable) {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 id_,
                 "SetREMBStatus(enable)");
  } else {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 id_,
                 "SetREMBStatus(disable)");
  }
  return rtcp_sender_.SetREMBStatus(enable);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetREMBData(const WebRtc_UWord32 bitrate,
                                             const WebRtc_UWord8 number_of_ssrc,
                                             const WebRtc_UWord32* ssrc) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
               "SetREMBData(bitrate:%d,?,?)", bitrate);
  return rtcp_sender_.SetREMBData(bitrate, number_of_ssrc, ssrc);
}

// (IJ) Extended jitter report.
bool ModuleRtpRtcpImpl::IJ() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "IJ()");

  return rtcp_sender_.IJ();
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetIJStatus(const bool enable) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetIJStatus(%s)", enable ? "true" : "false");

  return rtcp_sender_.SetIJStatus(enable);
}

WebRtc_Word32 ModuleRtpRtcpImpl::RegisterSendRtpHeaderExtension(
    const RTPExtensionType type,
    const WebRtc_UWord8 id) {
  return rtp_sender_.RegisterRtpHeaderExtension(type, id);
}

WebRtc_Word32 ModuleRtpRtcpImpl::DeregisterSendRtpHeaderExtension(
    const RTPExtensionType type) {
  return rtp_sender_.DeregisterRtpHeaderExtension(type);
}

WebRtc_Word32 ModuleRtpRtcpImpl::RegisterReceiveRtpHeaderExtension(
    const RTPExtensionType type,
    const WebRtc_UWord8 id) {
  return rtp_receiver_->RegisterRtpHeaderExtension(type, id);
}

WebRtc_Word32 ModuleRtpRtcpImpl::DeregisterReceiveRtpHeaderExtension(
  const RTPExtensionType type) {
  return rtp_receiver_->DeregisterRtpHeaderExtension(type);
}

// (TMMBR) Temporary Max Media Bit Rate.
bool ModuleRtpRtcpImpl::TMMBR() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "TMMBR()");

  return rtcp_sender_.TMMBR();
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetTMMBRStatus(const bool enable) {
  if (enable) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
                 "SetTMMBRStatus(enable)");
  } else {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
                 "SetTMMBRStatus(disable)");
  }
  return rtcp_sender_.SetTMMBRStatus(enable);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetTMMBN(const TMMBRSet* bounding_set) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SetTMMBN()");

  WebRtc_UWord32 max_bitrate_kbit =
      rtp_sender_.MaxConfiguredBitrateVideo() / 1000;
  return rtcp_sender_.SetTMMBN(bounding_set, max_bitrate_kbit);
}

// (NACK) Negative acknowledgment.

// Is Negative acknowledgment requests on/off?
NACKMethod ModuleRtpRtcpImpl::NACK() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "NACK()");

  NACKMethod child_method = kNackOff;
  const bool default_instance(child_modules_.empty() ? false : true);
  if (default_instance) {
    // For default we need to check all child modules too.
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());
    std::list<ModuleRtpRtcpImpl*>::const_iterator it =
      child_modules_.begin();
    while (it != child_modules_.end()) {
      RtpRtcp* module = *it;
      if (module) {
        NACKMethod nackMethod = module->NACK();
        if (nackMethod != kNackOff) {
          child_method = nackMethod;
          break;
        }
      }
      it++;
    }
  }

  NACKMethod method = nack_method_;
  if (child_method != kNackOff) {
    method = child_method;
  }
  return method;
}

// Turn negative acknowledgment requests on/off.
WebRtc_Word32 ModuleRtpRtcpImpl::SetNACKStatus(
    NACKMethod method, int max_reordering_threshold) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetNACKStatus(%u)", method);

  nack_method_ = method;
  rtp_receiver_->SetNACKStatus(method, max_reordering_threshold);
  return 0;
}

// Returns the currently configured retransmission mode.
int ModuleRtpRtcpImpl::SelectiveRetransmissions() const {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SelectiveRetransmissions()");
  return rtp_sender_.SelectiveRetransmissions();
}

// Enable or disable a retransmission mode, which decides which packets will
// be retransmitted if NACKed.
int ModuleRtpRtcpImpl::SetSelectiveRetransmissions(uint8_t settings) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetSelectiveRetransmissions(%u)",
               settings);
  return rtp_sender_.SetSelectiveRetransmissions(settings);
}

// Send a Negative acknowledgment packet.
WebRtc_Word32 ModuleRtpRtcpImpl::SendNACK(const WebRtc_UWord16* nack_list,
                                          const WebRtc_UWord16 size) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SendNACK(size:%u)", size);

  WebRtc_UWord16 avg_rtt = 0;
  rtcp_receiver_.RTT(rtp_receiver_->SSRC(), NULL, &avg_rtt, NULL, NULL);

  WebRtc_Word64 wait_time = 5 + ((avg_rtt * 3) >> 1);  // 5 + RTT * 1.5.
  if (wait_time == 5) {
    wait_time = 100;  // During startup we don't have an RTT.
  }
  const WebRtc_Word64 now = clock_->TimeInMilliseconds();
  const WebRtc_Word64 time_limit = now - wait_time;
  WebRtc_UWord16 nackLength = size;
  WebRtc_UWord16 start_id = 0;

  if (nack_last_time_sent_ < time_limit) {
    // Send list.
  } else {
    // Only send if extended list.
    if (nack_last_seq_number_sent_ == nack_list[size - 1]) {
      // Last seq num is the same don't send list.
      return 0;
    } else {
      // Send NACKs only for new sequence numbers to avoid re-sending
      // NACKs for sequences we have already sent.
      for (int i = 0; i < size; ++i)  {
        if (nack_last_seq_number_sent_ == nack_list[i]) {
          start_id = i + 1;
          break;
        }
      }
      nackLength = size - start_id;
    }
  }
  nack_last_time_sent_ =  now;
  nack_last_seq_number_sent_ = nack_list[size - 1];

  switch (nack_method_) {
    case kNackRtcp:
      return rtcp_sender_.SendRTCP(kRtcpNack, nackLength, &nack_list[start_id]);
    case kNackOff:
      return -1;
  };
  return -1;
}

// Store the sent packets, needed to answer to a Negative acknowledgment
// requests.
WebRtc_Word32 ModuleRtpRtcpImpl::SetStorePacketsStatus(
    const bool enable,
    const WebRtc_UWord16 number_to_store) {
  if (enable) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
                 "SetStorePacketsStatus(enable, number_to_store:%d)",
                 number_to_store);
  } else {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
                 "SetStorePacketsStatus(disable)");
  }
  rtp_sender_.SetStorePacketsStatus(enable, number_to_store);
  return 0;  // TODO(pwestin): change to void.
}

// Out-band TelephoneEvent detection.
WebRtc_Word32 ModuleRtpRtcpImpl::SetTelephoneEventStatus(
    const bool enable,
    const bool forward_to_decoder,
    const bool detect_end_of_tone) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
               "SetTelephoneEventStatus(enable:%d forward_to_decoder:%d"
               " detect_end_of_tone:%d)", enable, forward_to_decoder,
               detect_end_of_tone);

  assert(audio_);
  assert(rtp_telephone_event_handler_);
  return rtp_telephone_event_handler_->SetTelephoneEventStatus(
           enable, forward_to_decoder, detect_end_of_tone);
}

// Is out-band TelephoneEvent turned on/off?
bool ModuleRtpRtcpImpl::TelephoneEvent() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "TelephoneEvent()");

  assert(audio_);
  assert(rtp_telephone_event_handler_);
  return rtp_telephone_event_handler_->TelephoneEvent();
}

// Is forwarding of out-band telephone events turned on/off?
bool ModuleRtpRtcpImpl::TelephoneEventForwardToDecoder() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
               "TelephoneEventForwardToDecoder()");

  assert(audio_);
  assert(rtp_telephone_event_handler_);
  return rtp_telephone_event_handler_->TelephoneEventForwardToDecoder();
}

// Send a TelephoneEvent tone using RFC 2833 (4733).
WebRtc_Word32 ModuleRtpRtcpImpl::SendTelephoneEventOutband(
    const WebRtc_UWord8 key,
    const WebRtc_UWord16 time_ms,
    const WebRtc_UWord8 level) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
               "SendTelephoneEventOutband(key:%u, time_ms:%u, level:%u)", key,
               time_ms, level);

  return rtp_sender_.SendTelephoneEvent(key, time_ms, level);
}

bool ModuleRtpRtcpImpl::SendTelephoneEventActive(
    WebRtc_Word8& telephone_event) const {

  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SendTelephoneEventActive()");

  return rtp_sender_.SendTelephoneEventActive(&telephone_event);
}

// Set audio packet size, used to determine when it's time to send a DTMF
// packet in silence (CNG).
WebRtc_Word32 ModuleRtpRtcpImpl::SetAudioPacketSize(
    const WebRtc_UWord16 packet_size_samples) {

  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetAudioPacketSize(%u)",
               packet_size_samples);

  return rtp_sender_.SetAudioPacketSize(packet_size_samples);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetRTPAudioLevelIndicationStatus(
    const bool enable,
    const WebRtc_UWord8 id) {

  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetRTPAudioLevelIndicationStatus(enable=%d, ID=%u)",
               enable,
               id);

  if (enable) {
    rtp_receiver_->RegisterRtpHeaderExtension(kRtpExtensionAudioLevel, id);
  } else {
    rtp_receiver_->DeregisterRtpHeaderExtension(kRtpExtensionAudioLevel);
  }
  return rtp_sender_.SetAudioLevelIndicationStatus(enable, id);
}

WebRtc_Word32 ModuleRtpRtcpImpl::GetRTPAudioLevelIndicationStatus(
    bool& enable,
    WebRtc_UWord8& id) const {

  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "GetRTPAudioLevelIndicationStatus()");
  return rtp_sender_.AudioLevelIndicationStatus(&enable, &id);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetAudioLevel(
    const WebRtc_UWord8 level_d_bov) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetAudioLevel(level_d_bov:%u)",
               level_d_bov);
  return rtp_sender_.SetAudioLevel(level_d_bov);
}

// Set payload type for Redundant Audio Data RFC 2198.
WebRtc_Word32 ModuleRtpRtcpImpl::SetSendREDPayloadType(
    const WebRtc_Word8 payload_type) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetSendREDPayloadType(%d)",
               payload_type);

  return rtp_sender_.SetRED(payload_type);
}

// Get payload type for Redundant Audio Data RFC 2198.
WebRtc_Word32 ModuleRtpRtcpImpl::SendREDPayloadType(
    WebRtc_Word8& payload_type) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SendREDPayloadType()");

  return rtp_sender_.RED(&payload_type);
}

RtpVideoCodecTypes ModuleRtpRtcpImpl::ReceivedVideoCodec() const {
  return rtp_receiver_->VideoCodecType();
}

RtpVideoCodecTypes ModuleRtpRtcpImpl::SendVideoCodec() const {
  return rtp_sender_.VideoCodecType();
}

void ModuleRtpRtcpImpl::SetTargetSendBitrate(const uint32_t bitrate) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
               "SetTargetSendBitrate: %ubit", bitrate);

  const bool have_child_modules(child_modules_.empty() ? false : true);
  if (have_child_modules) {
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());
    if (simulcast_) {
      uint32_t bitrate_remainder = bitrate;
      std::list<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
      for (int i = 0; it != child_modules_.end() &&
           i < send_video_codec_.numberOfSimulcastStreams; ++it) {
        if ((*it)->SendingMedia()) {
          RTPSender& rtp_sender = (*it)->rtp_sender_;
          if (send_video_codec_.simulcastStream[i].maxBitrate * 1000 >
              bitrate_remainder) {
            rtp_sender.SetTargetSendBitrate(bitrate_remainder);
            bitrate_remainder = 0;
          } else {
            rtp_sender.SetTargetSendBitrate(
              send_video_codec_.simulcastStream[i].maxBitrate * 1000);
            bitrate_remainder -=
              send_video_codec_.simulcastStream[i].maxBitrate * 1000;
          }
          ++i;
        }
      }
    } else {
      std::list<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
      for (; it != child_modules_.end(); ++it) {
        RTPSender& rtp_sender = (*it)->rtp_sender_;
        rtp_sender.SetTargetSendBitrate(bitrate);
      }
    }
  } else {
    rtp_sender_.SetTargetSendBitrate(bitrate);
  }
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetKeyFrameRequestMethod(
    const KeyFrameRequestMethod method) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetKeyFrameRequestMethod(method:%u)",
               method);

  key_frame_req_method_ = method;
  return 0;
}

WebRtc_Word32 ModuleRtpRtcpImpl::RequestKeyFrame() {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "RequestKeyFrame");

  switch (key_frame_req_method_) {
    case kKeyFrameReqFirRtp:
      return rtp_sender_.SendRTPIntraRequest();
    case kKeyFrameReqPliRtcp:
      return rtcp_sender_.SendRTCP(kRtcpPli);
    case kKeyFrameReqFirRtcp:
      return rtcp_sender_.SendRTCP(kRtcpFir);
  }
  return -1;
}

WebRtc_Word32 ModuleRtpRtcpImpl::SendRTCPSliceLossIndication(
    const WebRtc_UWord8 picture_id) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SendRTCPSliceLossIndication (picture_id:%d)",
               picture_id);
  return rtcp_sender_.SendRTCP(kRtcpSli, 0, 0, false, picture_id);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetCameraDelay(const WebRtc_Word32 delay_ms) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetCameraDelay(%d)",
               delay_ms);
  const bool default_instance(child_modules_.empty() ? false : true);

  if (default_instance) {
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());

    std::list<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
    while (it != child_modules_.end()) {
      RtpRtcp* module = *it;
      if (module) {
        module->SetCameraDelay(delay_ms);
      }
      it++;
    }
    return 0;
  }
  return rtcp_sender_.SetCameraDelay(delay_ms);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetGenericFECStatus(
    const bool enable,
    const WebRtc_UWord8 payload_type_red,
    const WebRtc_UWord8 payload_type_fec) {
  if (enable) {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 id_,
                 "SetGenericFECStatus(enable, %u)",
                 payload_type_red);
  } else {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 id_,
                 "SetGenericFECStatus(disable)");
  }
  return rtp_sender_.SetGenericFECStatus(enable,
                                         payload_type_red,
                                         payload_type_fec);
}

WebRtc_Word32 ModuleRtpRtcpImpl::GenericFECStatus(
    bool& enable,
    WebRtc_UWord8& payload_type_red,
    WebRtc_UWord8& payload_type_fec) {

  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "GenericFECStatus()");

  bool child_enabled = false;
  const bool default_instance(child_modules_.empty() ? false : true);
  if (default_instance) {
    // For default we need to check all child modules too.
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());
    std::list<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
    while (it != child_modules_.end()) {
      RtpRtcp* module = *it;
      if (module)  {
        bool enabled = false;
        WebRtc_UWord8 dummy_ptype_red = 0;
        WebRtc_UWord8 dummy_ptype_fec = 0;
        if (module->GenericFECStatus(enabled,
                                     dummy_ptype_red,
                                     dummy_ptype_fec) == 0 && enabled) {
          child_enabled = true;
          break;
        }
      }
      it++;
    }
  }
  WebRtc_Word32 ret_val = rtp_sender_.GenericFECStatus(&enable,
                                                       &payload_type_red,
                                                       &payload_type_fec);
  if (child_enabled) {
    // Returns true if enabled for any child module.
    enable = child_enabled;
  }
  return ret_val;
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetFecParameters(
    const FecProtectionParams* delta_params,
    const FecProtectionParams* key_params) {
  const bool default_instance(child_modules_.empty() ? false : true);
  if (default_instance)  {
    // For default we need to update all child modules too.
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());

    std::list<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
    while (it != child_modules_.end()) {
      RtpRtcp* module = *it;
      if (module) {
        module->SetFecParameters(delta_params, key_params);
      }
      it++;
    }
    return 0;
  }
  return rtp_sender_.SetFecParameters(delta_params, key_params);
}

void ModuleRtpRtcpImpl::SetRemoteSSRC(const WebRtc_UWord32 ssrc) {
  // Inform about the incoming SSRC.
  rtcp_sender_.SetRemoteSSRC(ssrc);
  rtcp_receiver_.SetRemoteSSRC(ssrc);

  // Check for a SSRC collision.
  if (rtp_sender_.SSRC() == ssrc && !collision_detected_) {
    // If we detect a collision change the SSRC but only once.
    collision_detected_ = true;
    WebRtc_UWord32 new_ssrc = rtp_sender_.GenerateNewSSRC();
    if (new_ssrc == 0) {
      // Configured via API ignore.
      return;
    }
    if (kRtcpOff != rtcp_sender_.Status()) {
      // Send RTCP bye on the current SSRC.
      rtcp_sender_.SendRTCP(kRtcpBye);
    }
    // Change local SSRC and inform all objects about the new SSRC.
    rtcp_sender_.SetSSRC(new_ssrc);
    rtcp_receiver_.SetSSRC(new_ssrc);
  }
}

WebRtc_UWord32 ModuleRtpRtcpImpl::BitrateReceivedNow() const {
  return rtp_receiver_->BitrateNow();
}

void ModuleRtpRtcpImpl::BitrateSent(WebRtc_UWord32* total_rate,
                                    WebRtc_UWord32* video_rate,
                                    WebRtc_UWord32* fec_rate,
                                    WebRtc_UWord32* nack_rate) const {
  const bool default_instance(child_modules_.empty() ? false : true);

  if (default_instance) {
    // For default we need to update the send bitrate.
    CriticalSectionScoped lock(critical_section_module_ptrs_feedback_.get());

    if (total_rate != NULL)
      *total_rate = 0;
    if (video_rate != NULL)
      *video_rate = 0;
    if (fec_rate != NULL)
      *fec_rate = 0;
    if (nack_rate != NULL)
      *nack_rate = 0;

    std::list<ModuleRtpRtcpImpl*>::const_iterator it =
      child_modules_.begin();
    while (it != child_modules_.end()) {
      RtpRtcp* module = *it;
      if (module) {
        WebRtc_UWord32 child_total_rate = 0;
        WebRtc_UWord32 child_video_rate = 0;
        WebRtc_UWord32 child_fec_rate = 0;
        WebRtc_UWord32 child_nack_rate = 0;
        module->BitrateSent(&child_total_rate,
                            &child_video_rate,
                            &child_fec_rate,
                            &child_nack_rate);
        if (total_rate != NULL && child_total_rate > *total_rate)
          *total_rate = child_total_rate;
        if (video_rate != NULL && child_video_rate > *video_rate)
          *video_rate = child_video_rate;
        if (fec_rate != NULL && child_fec_rate > *fec_rate)
          *fec_rate = child_fec_rate;
        if (nack_rate != NULL && child_nack_rate > *nack_rate)
          *nack_rate = child_nack_rate;
      }
      it++;
    }
    return;
  }
  if (total_rate != NULL)
    *total_rate = rtp_sender_.BitrateLast();
  if (video_rate != NULL)
    *video_rate = rtp_sender_.VideoBitrateSent();
  if (fec_rate != NULL)
    *fec_rate = rtp_sender_.FecOverheadRate();
  if (nack_rate != NULL)
    *nack_rate = rtp_sender_.NackOverheadRate();
}

// Bad state of RTP receiver request a keyframe.
void ModuleRtpRtcpImpl::OnRequestIntraFrame() {
  RequestKeyFrame();
}

void ModuleRtpRtcpImpl::OnRequestSendReport() {
  rtcp_sender_.SendRTCP(kRtcpSr);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SendRTCPReferencePictureSelection(
    const WebRtc_UWord64 picture_id) {
  return rtcp_sender_.SendRTCP(kRtcpRpsi, 0, 0, false, picture_id);
}

WebRtc_UWord32 ModuleRtpRtcpImpl::SendTimeOfSendReport(
    const WebRtc_UWord32 send_report) {
  return rtcp_sender_.SendTimeOfSendReport(send_report);
}

void ModuleRtpRtcpImpl::OnReceivedNACK(
    const std::list<uint16_t>& nack_sequence_numbers) {
  if (!rtp_sender_.StorePackets() ||
      nack_sequence_numbers.size() == 0) {
    return;
  }
  WebRtc_UWord16 avg_rtt = 0;
  rtcp_receiver_.RTT(rtp_receiver_->SSRC(), NULL, &avg_rtt, NULL, NULL);
  rtp_sender_.OnReceivedNACK(nack_sequence_numbers, avg_rtt);
}

WebRtc_Word32 ModuleRtpRtcpImpl::LastReceivedNTP(
    WebRtc_UWord32& rtcp_arrival_time_secs,  // When we got the last report.
    WebRtc_UWord32& rtcp_arrival_time_frac,
    WebRtc_UWord32& remote_sr) {
  // Remote SR: NTP inside the last received (mid 16 bits from sec and frac).
  WebRtc_UWord32 ntp_secs = 0;
  WebRtc_UWord32 ntp_frac = 0;

  if (-1 == rtcp_receiver_.NTP(&ntp_secs,
                               &ntp_frac,
                               &rtcp_arrival_time_secs,
                               &rtcp_arrival_time_frac,
                               NULL)) {
    return -1;
  }
  remote_sr = ((ntp_secs & 0x0000ffff) << 16) + ((ntp_frac & 0xffff0000) >> 16);
  return 0;
}

bool ModuleRtpRtcpImpl::UpdateRTCPReceiveInformationTimers() {
  // If this returns true this channel has timed out.
  // Periodically check if this is true and if so call UpdateTMMBR.
  return rtcp_receiver_.UpdateRTCPReceiveInformationTimers();
}

// Called from RTCPsender.
WebRtc_Word32 ModuleRtpRtcpImpl::BoundingSet(bool& tmmbr_owner,
                                             TMMBRSet*& bounding_set) {
  return rtcp_receiver_.BoundingSet(tmmbr_owner, bounding_set);
}

int64_t ModuleRtpRtcpImpl::RtcpReportInterval() {
  if (audio_)
    return RTCP_INTERVAL_AUDIO_MS;
  else
    return RTCP_INTERVAL_VIDEO_MS;
}
}  // Namespace webrtc
