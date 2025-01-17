// libjingle
// Copyright 2004 Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. The name of the author may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef TALK_SESSION_PHONE_FAKEWEBRTCVIDEOCAPTUREMODULE_H_
#define TALK_SESSION_PHONE_FAKEWEBRTCVIDEOCAPTUREMODULE_H_

#include <vector>

#include "media/base/testutils.h"
#include "media/webrtc/fakewebrtcdeviceinfo.h"
#include "media/webrtc/webrtcvideocapturer.h"

class FakeWebRtcVcmFactory;

// Fake class for mocking out webrtc::VideoCaptureModule.
class FakeWebRtcVideoCaptureModule : public webrtc::VideoCaptureModule {
 public:
  FakeWebRtcVideoCaptureModule(FakeWebRtcVcmFactory* factory, WebRtc_Word32 id)
      : factory_(factory),
        id_(id),
        callback_(NULL),
        running_(false),
        delay_(0) {
  }
  virtual int32_t Version(char* version,
                          uint32_t& remaining_buffer_in_bytes,
                          uint32_t& position) const {
    return 0;
  }
  virtual int32_t TimeUntilNextProcess() {
    return 0;
  }
  virtual int32_t Process() {
    return 0;
  }
  virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id) {
    id_ = id;
    return 0;
  }
  virtual WebRtc_Word32 RegisterCaptureDataCallback(
      webrtc::VideoCaptureDataCallback& callback) {
    callback_ = &callback;
    return 0;
  }
  virtual WebRtc_Word32 DeRegisterCaptureDataCallback() {
    callback_ = NULL;
    return 0;
  }
  virtual WebRtc_Word32 RegisterCaptureCallback(
      webrtc::VideoCaptureFeedBack& callback) {
    return -1;  // not implemented
  }
  virtual WebRtc_Word32 DeRegisterCaptureCallback() {
    return 0;
  }
  virtual WebRtc_Word32 StartCapture(
      const webrtc::VideoCaptureCapability& cap) {
    if (running_) return -1;
    cap_ = cap;
    running_ = true;
    return 0;
  }
  virtual WebRtc_Word32 StopCapture() {
    running_ = false;
    return 0;
  }
  virtual const char* CurrentDeviceName() const {
    return NULL;  // not implemented
  }
  virtual bool CaptureStarted() {
    return running_;
  }
  virtual WebRtc_Word32 CaptureSettings(
      webrtc::VideoCaptureCapability& settings) {
    if (!running_) return -1;
    settings = cap_;
    return 0;
  }
  virtual WebRtc_Word32 SetCaptureDelay(WebRtc_Word32 delay) {
    delay_ = delay;
    return 0;
  }
  virtual WebRtc_Word32 CaptureDelay() {
    return delay_;
  }
  virtual WebRtc_Word32 SetCaptureRotation(
      webrtc::VideoCaptureRotation rotation) {
    return -1;  // not implemented
  }
  virtual VideoCaptureEncodeInterface* GetEncodeInterface(
      const webrtc::VideoCodec& codec) {
    return NULL;  // not implemented
  }
  virtual WebRtc_Word32 EnableFrameRateCallback(const bool enable) {
    return -1;  // not implemented
  }
  virtual WebRtc_Word32 EnableNoPictureAlarm(const bool enable) {
    return -1;  // not implemented
  }
  virtual int32_t AddRef() {
    return 0;
  }
  virtual int32_t Release() {
    delete this;
    return 0;
  }

  bool SendFrame(int w, int h) {
    if (!running_) return false;
    webrtc::I420VideoFrame sample;
    // Setting stride based on width.
    if (sample.CreateEmptyFrame(w, h, w, (w + 1) / 2, (w + 1) / 2) < 0) {
      return false;
    }
    if (callback_) {
      callback_->OnIncomingCapturedFrame(id_, sample);
    }
    return true;
  }

  const webrtc::VideoCaptureCapability& cap() const {
    return cap_;
  }

 private:
  // Ref-counted, use Release() instead.
  ~FakeWebRtcVideoCaptureModule();

  FakeWebRtcVcmFactory* factory_;
  int id_;
  webrtc::VideoCaptureDataCallback* callback_;
  bool running_;
  webrtc::VideoCaptureCapability cap_;
  int delay_;
};

#endif  // TALK_SESSION_PHONE_FAKEWEBRTCVIDEOCAPTUREMODULE_H_
