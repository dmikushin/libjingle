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

#ifndef TALK_APP_WEBRTC_DTMFSENDER_H_
#define TALK_APP_WEBRTC_DTMFSENDER_H_

#include <string>

#include "app/webrtc/dtmfsenderinterface.h"
#include "app/webrtc/mediastreaminterface.h"
#include "app/webrtc/proxy.h"
#include "base/common.h"
#include "base/messagehandler.h"
#include "base/refcount.h"

// DtmfSender is the native implementation of the RTCDTMFSender defined by
// the WebRTC W3C Editor's Draft.
// http://dev.w3.org/2011/webrtc/editor/webrtc.html

namespace talk_base {
class Thread;
}

namespace webrtc {

// This interface is called by DtmfSender to talk to the actual audio channel
// to send DTMF.
class DtmfProviderInterface {
 public:
  // Returns true if the audio track with given id (|track_id|) is capable
  // of sending DTMF. Otherwise returns false.
  virtual bool CanInsertDtmf(const std::string& track_id) = 0;
  // Sends DTMF |code| via the audio track with given id (|track_id|).
  // The |duration| indicates the length of the DTMF tone in ms.
  // Returns true on success and false on failure.
  virtual bool InsertDtmf(const std::string& track_id,
                          int code, int duration) = 0;

 protected:
  virtual ~DtmfProviderInterface() {}
};

class DtmfSender
    : public DtmfSenderInterface,
      public talk_base::MessageHandler {
 public:
  static talk_base::scoped_refptr<DtmfSender> Create(
      AudioTrackInterface* track,
      talk_base::Thread* signaling_thread,
      DtmfProviderInterface* provider);

  // Implements DtmfSenderInterface.
  virtual void RegisterObserver(DtmfSenderObserverInterface* observer) OVERRIDE;
  virtual void UnregisterObserver() OVERRIDE;
  virtual bool CanInsertDtmf() OVERRIDE;
  virtual bool InsertDtmf(const std::string& tones, int duration,
                          int inter_tone_gap) OVERRIDE;
  virtual const AudioTrackInterface* track() const OVERRIDE;
  virtual std::string tones() const OVERRIDE;
  virtual int duration() const OVERRIDE;
  virtual int inter_tone_gap() const OVERRIDE;

 protected:
  DtmfSender(AudioTrackInterface* track,
             talk_base::Thread* signaling_thread,
             DtmfProviderInterface* provider);
  virtual ~DtmfSender();

 private:
  DtmfSender();

  // Implements MessageHandler.
  virtual void OnMessage(talk_base::Message* msg);

  // The DTMF sending task.
  void DoInsertDtmf();

  talk_base::scoped_refptr<AudioTrackInterface> track_;
  DtmfSenderObserverInterface* observer_;
  talk_base::Thread* signaling_thread_;
  DtmfProviderInterface* provider_;
  std::string tones_;
  int duration_;
  int inter_tone_gap_;

  DISALLOW_COPY_AND_ASSIGN(DtmfSender);
};

// Define proxy for DtmfSenderInterface.
BEGIN_PROXY_MAP(DtmfSender)
  PROXY_METHOD1(void, RegisterObserver, DtmfSenderObserverInterface*)
  PROXY_METHOD0(void, UnregisterObserver)
  PROXY_METHOD0(bool, CanInsertDtmf)
  PROXY_METHOD3(bool, InsertDtmf, const std::string&, int, int)
  PROXY_CONSTMETHOD0(const AudioTrackInterface*, track)
  PROXY_CONSTMETHOD0(std::string, tones)
  PROXY_CONSTMETHOD0(int, duration)
  PROXY_CONSTMETHOD0(int, inter_tone_gap)
END_PROXY()

// Get DTMF code from the DTMF event character.
bool GetDtmfCode(char tone, int* code);

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_DTMFSENDER_H_
