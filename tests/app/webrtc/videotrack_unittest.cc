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

#include <string>

#include "app/webrtc/test/fakevideotrackrenderer.h"
#include "app/webrtc/videotrack.h"
#include "base/gunit.h"
#include "base/scoped_ptr.h"
#include "media/webrtc/webrtcvideoframe.h"

using webrtc::FakeVideoTrackRenderer;
using webrtc::VideoTrack;
using webrtc::VideoTrackInterface;

// Test adding renderers to a video track and render to them by providing
// VideoFrames to the track frame input.
TEST(VideoTrack, RenderVideo) {
  static const char kVideoTrackId[] = "track_id";
  talk_base::scoped_refptr<VideoTrackInterface> video_track(
      VideoTrack::Create(kVideoTrackId, NULL));
  // FakeVideoTrackRenderer register itself to |video_track|
  talk_base::scoped_ptr<FakeVideoTrackRenderer> renderer_1(
      new FakeVideoTrackRenderer(video_track.get()));

  cricket::VideoRenderer* render_input = video_track->FrameInput();
  ASSERT_FALSE(render_input == NULL);

  render_input->SetSize(123, 123, 0);
  EXPECT_EQ(1, renderer_1->num_set_sizes());
  EXPECT_EQ(123, renderer_1->width());
  EXPECT_EQ(123, renderer_1->height());

  cricket::WebRtcVideoFrame frame;
  frame.InitToBlack(123, 123, 1, 1, 0, 0);
  render_input->RenderFrame(&frame);
  EXPECT_EQ(1, renderer_1->num_rendered_frames());

  // FakeVideoTrackRenderer register itself to |video_track|
  talk_base::scoped_ptr<FakeVideoTrackRenderer> renderer_2(
      new FakeVideoTrackRenderer(video_track.get()));

  render_input->RenderFrame(&frame);

  EXPECT_EQ(1, renderer_1->num_set_sizes());
  EXPECT_EQ(123, renderer_1->width());
  EXPECT_EQ(123, renderer_1->height());
  EXPECT_EQ(1, renderer_2->num_set_sizes());
  EXPECT_EQ(123, renderer_2->width());
  EXPECT_EQ(123, renderer_2->height());

  EXPECT_EQ(2, renderer_1->num_rendered_frames());
  EXPECT_EQ(1, renderer_2->num_rendered_frames());

  video_track->RemoveRenderer(renderer_1.get());
  render_input->RenderFrame(&frame);

  EXPECT_EQ(2, renderer_1->num_rendered_frames());
  EXPECT_EQ(2, renderer_2->num_rendered_frames());
}
