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

// This file contains interfaces for DataChannels
// http://dev.w3.org/2011/webrtc/editor/webrtc.html#rtcdatachannel

#ifndef TALK_APP_WEBRTC_DATACHANNELINTERFACE_H_
#define TALK_APP_WEBRTC_DATACHANNELINTERFACE_H_

#include <string>

#include "base/basictypes.h"
#include "base/buffer.h"
#include "base/refcount.h"


namespace webrtc {

struct DataChannelInit {
  bool reliable;
};

struct DataBuffer {
  DataBuffer() : binary(false) {
  }
  explicit DataBuffer(const std::string& string_to_send)
      : data(string_to_send.c_str(), string_to_send.length()),
        binary(false) {
  }
  talk_base::Buffer data;
  // Indicates if the receivied data contains UTF-8 or binary data.
  // Note that the upper layers are left to verify the UTF-8 encoding.
  bool binary;
};

class DataChannelObserver {
 public:
  // The data channel state have changed.
  virtual void OnStateChange() = 0;
  //  A data buffer was successfully received.
  virtual void OnMessage(const DataBuffer& buffer) = 0;

 protected:
  virtual ~DataChannelObserver() {}
};

class DataChannelInterface : public talk_base::RefCountInterface {
 public:
  enum DataState {
    kConnecting,
    kOpen,  // The DataChannel is ready to send data.
    kClosing,
    kClosed
  };

  virtual void RegisterObserver(DataChannelObserver* observer) = 0;
  virtual void UnregisterObserver() = 0;
  // The label attribute represents a label that can be used to distinguish this
  // DataChannel object from other DataChannel objects.
  virtual std::string label() const = 0;
  virtual bool reliable() const = 0;
  virtual DataState state() const = 0;
  // The buffered_amount returns the number of bytes of application data
  // (UTF-8 text and binary data) that have been queued using SendBuffer but
  // have not yet been transmitted to the network.
  virtual uint64 buffered_amount() const = 0;
  virtual void Close() = 0;
  // Sends |data| to the remote peer.
  virtual bool Send(const DataBuffer& buffer) = 0;

 protected:
  virtual ~DataChannelInterface() {}
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_DATACHANNELINTERFACE_H_
