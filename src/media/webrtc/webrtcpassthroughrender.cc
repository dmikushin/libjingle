/*
 * libjingle
 * Copyright 2004 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "media/webrtc/webrtcpassthroughrender.h"

#include "base/common.h"
#include "base/logging.h"

namespace cricket {

#define LOG_FIND_STREAM_ERROR(func, id) LOG(LS_ERROR) \
    << "" << func << " - Failed to find stream: " << id

class PassthroughStream: public webrtc::VideoRenderCallback {
 public:
  explicit PassthroughStream(const WebRtc_UWord32 stream_id)
      : stream_id_(stream_id),
        running_(false) {
  }
  virtual ~PassthroughStream() {
  }
  virtual WebRtc_Word32 RenderFrame(const WebRtc_UWord32 stream_id,
                                    webrtc::I420VideoFrame& videoFrame) {
    talk_base::CritScope cs(&stream_critical_);
    // Send frame for rendering directly
    if (running_ && renderer_) {
      renderer_->RenderFrame(stream_id, videoFrame);
    }
    return 0;
  }
  WebRtc_Word32 SetRenderer(VideoRenderCallback* renderer) {
    talk_base::CritScope cs(&stream_critical_);
    renderer_ = renderer;
    return 0;
  }

  WebRtc_Word32 StartRender() {
    talk_base::CritScope cs(&stream_critical_);
    running_ = true;
    return 0;
  }

  WebRtc_Word32 StopRender() {
    talk_base::CritScope cs(&stream_critical_);
    running_ = false;
    return 0;
  }

 private:
  WebRtc_UWord32 stream_id_;
  VideoRenderCallback* renderer_;
  talk_base::CriticalSection stream_critical_;
  bool running_;
};

WebRtcPassthroughRender::WebRtcPassthroughRender()
    : window_(NULL) {
}

WebRtcPassthroughRender::~WebRtcPassthroughRender() {
  while (!stream_render_map_.empty()) {
    PassthroughStream* stream = stream_render_map_.begin()->second;
    stream_render_map_.erase(stream_render_map_.begin());
    delete stream;
  }
}

webrtc::VideoRenderCallback* WebRtcPassthroughRender::AddIncomingRenderStream(
    const WebRtc_UWord32 stream_id,
    const WebRtc_UWord32 zOrder,
    const float left, const float top,
    const float right, const float bottom) {
  talk_base::CritScope cs(&render_critical_);
  // Stream already exist.
  if (FindStream(stream_id) != NULL) {
    LOG(LS_ERROR) << "AddIncomingRenderStream - Stream already exists: "
                  << stream_id;
    return NULL;
  }

  PassthroughStream* stream = new PassthroughStream(stream_id);
  // Store the stream
  stream_render_map_[stream_id] = stream;
  return stream;
}

WebRtc_Word32 WebRtcPassthroughRender::DeleteIncomingRenderStream(
    const WebRtc_UWord32 stream_id) {
  talk_base::CritScope cs(&render_critical_);
  PassthroughStream* stream = FindStream(stream_id);
  if (stream == NULL) {
    LOG_FIND_STREAM_ERROR("DeleteIncomingRenderStream", stream_id);
    return -1;
  }
  delete stream;
  stream_render_map_.erase(stream_id);
  return 0;
}

WebRtc_Word32 WebRtcPassthroughRender::AddExternalRenderCallback(
    const WebRtc_UWord32 stream_id,
    webrtc::VideoRenderCallback* render_object) {
  talk_base::CritScope cs(&render_critical_);
  PassthroughStream* stream = FindStream(stream_id);
  if (stream == NULL) {
    LOG_FIND_STREAM_ERROR("AddExternalRenderCallback", stream_id);
    return -1;
  }
  return stream->SetRenderer(render_object);
}

bool WebRtcPassthroughRender::HasIncomingRenderStream(
    const WebRtc_UWord32 stream_id) const {
  return (FindStream(stream_id) != NULL);
}

webrtc::RawVideoType WebRtcPassthroughRender::PreferredVideoType() const {
  return webrtc::kVideoI420;
}

WebRtc_Word32 WebRtcPassthroughRender::StartRender(
    const WebRtc_UWord32 stream_id) {
  talk_base::CritScope cs(&render_critical_);
  PassthroughStream* stream = FindStream(stream_id);
  if (stream == NULL) {
    LOG_FIND_STREAM_ERROR("StartRender", stream_id);
    return -1;
  }
  return stream->StartRender();
}

WebRtc_Word32 WebRtcPassthroughRender::StopRender(
    const WebRtc_UWord32 stream_id) {
  talk_base::CritScope cs(&render_critical_);
  PassthroughStream* stream = FindStream(stream_id);
  if (stream == NULL) {
    LOG_FIND_STREAM_ERROR("StopRender", stream_id);
    return -1;
  }
  return stream->StopRender();
}

// TODO(ronghuawu): Is it ok to return non-const pointer to PassthroughStream
// from this const function FindStream.
PassthroughStream* WebRtcPassthroughRender::FindStream(
    const WebRtc_UWord32 stream_id) const {
  StreamMap::const_iterator it = stream_render_map_.find(stream_id);
  if (it == stream_render_map_.end()) {
    return NULL;
  }
  return it->second;
}

}  // namespace cricket
