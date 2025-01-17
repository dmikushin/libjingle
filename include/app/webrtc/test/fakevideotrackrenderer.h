/*
 * libjingle
 * Copyright 2012, Google Inc.
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

#ifndef TALK_APP_WEBRTC_TEST_FAKEVIDEOTRACKRENDERER_H_
#define TALK_APP_WEBRTC_TEST_FAKEVIDEOTRACKRENDERER_H_

#include "app/webrtc/mediastreaminterface.h"
#include "media/base/fakevideorenderer.h"

namespace webrtc {

class FakeVideoTrackRenderer : public VideoRendererInterface {
 public:
  explicit FakeVideoTrackRenderer(VideoTrackInterface* video_track)
      : video_track_(video_track) {
    video_track_->AddRenderer(this);
  }
  ~FakeVideoTrackRenderer() {
    video_track_->RemoveRenderer(this);
  }

  // Implements VideoRendererInterface
  virtual void SetSize(int width, int height) {
    fake_renderer_.SetSize(width, height, 0);
  }

  virtual void RenderFrame(const cricket::VideoFrame* frame) {
    fake_renderer_.RenderFrame(frame);
  }

  int errors() const { return fake_renderer_.errors(); }
  int width() const { return fake_renderer_.width(); }
  int height() const { return fake_renderer_.height(); }
  int num_set_sizes() const { return fake_renderer_.num_set_sizes(); }
  int num_rendered_frames() const {
    return fake_renderer_.num_rendered_frames();
  }

 private:
  cricket::FakeVideoRenderer fake_renderer_;
  talk_base::scoped_refptr<VideoTrackInterface> video_track_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_TEST_FAKEVIDEOTRACKRENDERER_H_
