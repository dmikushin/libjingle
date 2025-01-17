/*
 * libjingle
 * Copyright 2011, Google Inc.
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

// This file contains the implementation of MediaStreamInterface interface.

#ifndef TALK_APP_WEBRTC_MEDIASTREAM_H_
#define TALK_APP_WEBRTC_MEDIASTREAM_H_

#include <string>
#include <vector>

#include "app/webrtc/mediastreaminterface.h"
#include "app/webrtc/notifier.h"

namespace webrtc {

class AudioMediaStreamTrackList;
class VideoMediaStreamTrackList;

class MediaStream : public Notifier<LocalMediaStreamInterface> {
 public:
  static talk_base::scoped_refptr<MediaStream> Create(const std::string& label);

  virtual std::string label() const OVERRIDE { return label_; }

  virtual bool AddTrack(AudioTrackInterface* track) OVERRIDE;
  virtual bool AddTrack(VideoTrackInterface* track) OVERRIDE;
  virtual bool RemoveTrack(AudioTrackInterface* track) OVERRIDE;
  virtual bool RemoveTrack(VideoTrackInterface* track) OVERRIDE;
  virtual talk_base::scoped_refptr<AudioTrackInterface>
      FindAudioTrack(const std::string& track_id);
  virtual talk_base::scoped_refptr<VideoTrackInterface>
      FindVideoTrack(const std::string& track_id);

  // TODO(perkj): Remove when there are no callers.
  virtual AudioTracks* audio_tracks() OVERRIDE;
  virtual VideoTracks* video_tracks() OVERRIDE;

  virtual AudioTrackVector GetAudioTracks() OVERRIDE { return audio_tracks_; }
  virtual VideoTrackVector GetVideoTracks() OVERRIDE { return video_tracks_; }

 protected:
  explicit MediaStream(const std::string& label);

 private:
  template <typename TrackVector, typename Track>
  bool AddTrack(TrackVector* Tracks, Track* track);
  template <typename TrackVector>
  bool RemoveTrack(TrackVector* Tracks, MediaStreamTrackInterface* track);

  std::string label_;
  AudioTrackVector audio_tracks_;
  VideoTrackVector video_tracks_;

  // TODO(perkj): Remove when there are no callers to audio_tracks and
  // video_tracks.
  talk_base::scoped_refptr <AudioMediaStreamTrackList> audio_track_list_;
  talk_base::scoped_refptr <VideoMediaStreamTrackList> video_track_list_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_MEDIASTREAM_H_
