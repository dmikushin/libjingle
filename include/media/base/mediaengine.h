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

#ifndef TALK_MEDIA_BASE_MEDIAENGINE_H_
#define TALK_MEDIA_BASE_MEDIAENGINE_H_

#ifdef OSX
#include <CoreAudio/CoreAudio.h>
#endif

#include <climits>
#include <string>
#include <vector>

#include "base/sigslotrepeater.h"
#include "media/base/codec.h"
#include "media/base/mediachannel.h"
#include "media/base/mediacommon.h"
#include "media/base/videocapturer.h"
#include "media/base/videocommon.h"
#include "media/base/videoprocessor.h"
#include "media/base/voiceprocessor.h"
#include "media/devices/devicemanager.h"

namespace cricket {

class VideoCapturer;

// MediaEngineInterface is an abstraction of a media engine which can be
// subclassed to support different media componentry backends.
// It supports voice and video operations in the same class to facilitate
// proper synchronization between both media types.
class MediaEngineInterface {
 public:
  // Bitmask flags for options that may be supported by the media engine
  // implementation.  This can be converted to and from an
  // AudioOptions struct for backwards compatibility with calls that
  // use flags until we transition to using structs everywhere.
  enum AudioFlags {
    // Audio processing that attempts to filter away the output signal from
    // later inbound pickup.
    ECHO_CANCELLATION         = 1 << 0,
    // Audio processing to adjust the sensitivity of the local mic dynamically.
    AUTO_GAIN_CONTROL         = 1 << 1,
    // Audio processing to filter out background noise.
    NOISE_SUPPRESSION         = 1 << 2,
    // Audio processing to remove background noise of lower frequencies.
    HIGHPASS_FILTER           = 1 << 3,
    // A switch to swap which captured signal is left and right in stereo mode.
    STEREO_FLIPPING           = 1 << 4,
    // Controls delegation echo cancellation to use the OS' facility.
    SYSTEM_AEC_MODE           = 1 << 5,

    ALL_AUDIO_OPTIONS         = (1 << 6) - 1,
    DEFAULT_AUDIO_OPTIONS     = ECHO_CANCELLATION | AUTO_GAIN_CONTROL |
                                NOISE_SUPPRESSION | HIGHPASS_FILTER,
  };

  // Default value to be used for SetAudioDelayOffset().
  static const int kDefaultAudioDelayOffset;

  virtual ~MediaEngineInterface() {}

  // Initialization
  // Starts the engine.
  virtual bool Init() = 0;
  // Shuts down the engine.
  virtual void Terminate() = 0;
  // Returns what the engine is capable of, as a set of Capabilities, above.
  virtual int GetCapabilities() = 0;

  // MediaChannel creation
  // Creates a voice media channel. Returns NULL on failure.
  virtual VoiceMediaChannel *CreateChannel() = 0;
  // Creates a video media channel, paired with the specified voice channel.
  // Returns NULL on failure.
  virtual VideoMediaChannel *CreateVideoChannel(
      VoiceMediaChannel* voice_media_channel) = 0;

  // Creates a soundclip object for playing sounds on. Returns NULL on failure.
  virtual SoundclipMedia *CreateSoundclip() = 0;

  // Configuration
  // Sets global audio options. "options" are from AudioOptions, above.
  virtual bool SetAudioOptions(int options) = 0;
  // Sets global video options. "options" are from VideoOptions, above.
  virtual bool SetVideoOptions(int options) = 0;
  // Sets the value used by the echo canceller to offset delay values obtained
  // from the OS.
  virtual bool SetAudioDelayOffset(int offset) = 0;
  // Sets the default (maximum) codec/resolution and encoder option to capture
  // and encode video.
  virtual bool SetDefaultVideoEncoderConfig(const VideoEncoderConfig& config)
      = 0;

  // Device selection
  // TODO(tschmelcher): Add method for selecting the soundclip device.
  virtual bool SetSoundDevices(const Device* in_device,
                               const Device* out_device) = 0;
  // Sets the externally provided video capturer. The ssrc is the ssrc of the
  // (video) stream for which the video capturer should be set.
  virtual bool SetVideoCapturer(VideoCapturer* capturer) = 0;
  virtual VideoCapturer* GetVideoCapturer() const = 0;

  // Device configuration
  // Gets the current speaker volume, as a value between 0 and 255.
  virtual bool GetOutputVolume(int* level) = 0;
  // Sets the current speaker volume, as a value between 0 and 255.
  virtual bool SetOutputVolume(int level) = 0;

  // Local monitoring
  // Gets the current microphone level, as a value between 0 and 10.
  virtual int GetInputLevel() = 0;
  // Starts or stops the local microphone. Useful if local mic info is needed
  // prior to a call being connected; the mic will be started automatically
  // when a VoiceMediaChannel starts sending.
  virtual bool SetLocalMonitor(bool enable) = 0;
  // Installs a callback for raw frames from the local camera.
  virtual bool SetLocalRenderer(VideoRenderer* renderer) = 0;
  // Starts/stops local camera.
  virtual bool SetVideoCapture(bool capture) = 0;

  virtual const std::vector<AudioCodec>& audio_codecs() = 0;
  virtual const std::vector<VideoCodec>& video_codecs() = 0;

  // Logging control
  virtual void SetVoiceLogging(int min_sev, const char* filter) = 0;
  virtual void SetVideoLogging(int min_sev, const char* filter) = 0;

  // media processors for effects
  virtual bool RegisterVideoProcessor(VideoProcessor* video_processor) = 0;
  virtual bool UnregisterVideoProcessor(VideoProcessor* video_processor) = 0;
  virtual bool RegisterVoiceProcessor(uint32 ssrc,
                                      VoiceProcessor* video_processor,
                                      MediaProcessorDirection direction) = 0;
  virtual bool UnregisterVoiceProcessor(uint32 ssrc,
                                        VoiceProcessor* video_processor,
                                        MediaProcessorDirection direction) = 0;


  virtual VideoFormat GetStartCaptureFormat() const = 0;

  sigslot::repeater2<VideoCapturer*, CaptureState>
      SignalVideoCaptureStateChange;
};


class MediaEngineFactory {
 public:
  static MediaEngineInterface* Create();
};

// CompositeMediaEngine constructs a MediaEngine from separate
// voice and video engine classes.
template<class VOICE, class VIDEO>
class CompositeMediaEngine : public MediaEngineInterface {
 public:
  CompositeMediaEngine() {}
  virtual ~CompositeMediaEngine() {}
  virtual bool Init() {
    if (!voice_.Init())
      return false;
    if (!video_.Init()) {
      voice_.Terminate();
      return false;
    }
    SignalVideoCaptureStateChange.repeat(video_.SignalCaptureStateChange);
    return true;
  }
  virtual void Terminate() {
    video_.Terminate();
    voice_.Terminate();
  }

  virtual int GetCapabilities() {
    return (voice_.GetCapabilities() | video_.GetCapabilities());
  }
  virtual VoiceMediaChannel *CreateChannel() {
    return voice_.CreateChannel();
  }
  virtual VideoMediaChannel *CreateVideoChannel(VoiceMediaChannel* channel) {
    return video_.CreateChannel(channel);
  }
  virtual SoundclipMedia *CreateSoundclip() {
    return voice_.CreateSoundclip();
  }

  virtual bool SetAudioOptions(int o) {
    return voice_.SetOptions(o);
  }
  virtual bool SetVideoOptions(int o) {
    return video_.SetOptions(o);
  }
  virtual bool SetAudioDelayOffset(int offset) {
    return voice_.SetDelayOffset(offset);
  }
  virtual bool SetDefaultVideoEncoderConfig(const VideoEncoderConfig& config) {
    return video_.SetDefaultEncoderConfig(config);
  }

  virtual bool SetSoundDevices(const Device* in_device,
                               const Device* out_device) {
    return voice_.SetDevices(in_device, out_device);
  }
  virtual bool SetVideoCapturer(VideoCapturer* capturer) {
    return video_.SetVideoCapturer(capturer);
  }
  virtual VideoCapturer* GetVideoCapturer() const {
    return video_.GetVideoCapturer();
  }

  virtual bool GetOutputVolume(int* level) {
    return voice_.GetOutputVolume(level);
  }
  virtual bool SetOutputVolume(int level) {
    return voice_.SetOutputVolume(level);
  }

  virtual int GetInputLevel() {
    return voice_.GetInputLevel();
  }
  virtual bool SetLocalMonitor(bool enable) {
    return voice_.SetLocalMonitor(enable);
  }
  virtual bool SetLocalRenderer(VideoRenderer* renderer) {
    return video_.SetLocalRenderer(renderer);
  }
  virtual bool SetVideoCapture(bool capture) {
    return video_.SetCapture(capture);
  }

  virtual const std::vector<AudioCodec>& audio_codecs() {
    return voice_.codecs();
  }
  virtual const std::vector<VideoCodec>& video_codecs() {
    return video_.codecs();
  }

  virtual void SetVoiceLogging(int min_sev, const char* filter) {
    return voice_.SetLogging(min_sev, filter);
  }
  virtual void SetVideoLogging(int min_sev, const char* filter) {
    return video_.SetLogging(min_sev, filter);
  }

  virtual bool RegisterVideoProcessor(VideoProcessor* processor) {
    return video_.RegisterProcessor(processor);
  }
  virtual bool UnregisterVideoProcessor(VideoProcessor* processor) {
    return video_.UnregisterProcessor(processor);
  }
  virtual bool RegisterVoiceProcessor(uint32 ssrc,
                                      VoiceProcessor* processor,
                                      MediaProcessorDirection direction) {
    return voice_.RegisterProcessor(ssrc, processor, direction);
  }
  virtual bool UnregisterVoiceProcessor(uint32 ssrc,
                                        VoiceProcessor* processor,
                                        MediaProcessorDirection direction) {
    return voice_.UnregisterProcessor(ssrc, processor, direction);
  }
  virtual VideoFormat GetStartCaptureFormat() const {
    return video_.GetStartCaptureFormat();
  }

 protected:
  VOICE voice_;
  VIDEO video_;
};

// NullVoiceEngine can be used with CompositeMediaEngine in the case where only
// a video engine is desired.
class NullVoiceEngine {
 public:
  bool Init() { return true; }
  void Terminate() {}
  int GetCapabilities() { return 0; }
  // If you need this to return an actual channel, use FakeMediaEngine instead.
  VoiceMediaChannel* CreateChannel() {
    return NULL;
  }
  SoundclipMedia* CreateSoundclip() {
    return NULL;
  }
  bool SetDelayOffset(int offset) { return true; }
  bool SetOptions(int opts) { return true; }
  bool SetDevices(const Device* in_device, const Device* out_device) {
    return true;
  }
  bool GetOutputVolume(int* level) {
    *level = 0;
    return true;
  }
  bool SetOutputVolume(int level) { return true; }
  int GetInputLevel() { return 0; }
  bool SetLocalMonitor(bool enable) { return true; }
  const std::vector<AudioCodec>& codecs() { return codecs_; }
  void SetLogging(int min_sev, const char* filter) {}
  bool RegisterProcessor(uint32 ssrc,
                         VoiceProcessor* voice_processor,
                         MediaProcessorDirection direction) { return true; }
  bool UnregisterProcessor(uint32 ssrc,
                           VoiceProcessor* voice_processor,
                           MediaProcessorDirection direction) { return true; }

 private:
  std::vector<AudioCodec> codecs_;
};

// NullVideoEngine can be used with CompositeMediaEngine in the case where only
// a voice engine is desired.
class NullVideoEngine {
 public:
  bool Init() { return true; }
  void Terminate() {}
  int GetCapabilities() { return 0; }
  // If you need this to return an actual channel, use FakeMediaEngine instead.
  VideoMediaChannel* CreateChannel(
      VoiceMediaChannel* voice_media_channel) {
    return NULL;
  }
  bool SetOptions(int opts) { return true; }
  bool SetDefaultEncoderConfig(const VideoEncoderConfig& config) {
    return true;
  }
  bool SetLocalRenderer(VideoRenderer* renderer) { return true; }
  bool SetCapture(bool capture) { return true;  }
  const std::vector<VideoCodec>& codecs() { return codecs_; }
  void SetLogging(int min_sev, const char* filter) {}
  bool RegisterProcessor(VideoProcessor* video_processor) { return true; }
  bool UnregisterProcessor(VideoProcessor* video_processor) { return true; }
  VideoFormat GetStartCaptureFormat() const { return VideoFormat(); }
  bool SetVideoCapturer(VideoCapturer* capturer) { return true; }
  VideoCapturer* GetVideoCapturer() const { return NULL; }

  sigslot::signal2<VideoCapturer*, CaptureState> SignalCaptureStateChange;
 private:
  std::vector<VideoCodec> codecs_;
};

typedef CompositeMediaEngine<NullVoiceEngine, NullVideoEngine> NullMediaEngine;

class DataEngineInterface {
 public:
  virtual ~DataEngineInterface() {}
  virtual DataMediaChannel* CreateChannel() = 0;
  virtual const std::vector<DataCodec>& data_codecs() = 0;
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_MEDIAENGINE_H_
