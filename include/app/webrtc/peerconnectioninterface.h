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

// This file contains the PeerConnection interface as defined in
// http://dev.w3.org/2011/webrtc/editor/webrtc.html#peer-to-peer-connections.
// Applications must use this interface to implement peerconnection.
// PeerConnectionFactory class provides factory methods to create
// peerconnection, mediastream and media tracks objects.
//
// The Following steps are needed to setup a typical call using Jsep.
// 1. Create a PeerConnectionFactoryInterface. Check constructors for more
// information about input parameters.
// 2. Create a PeerConnection object. Provide a configuration string which
// points either to stun or turn server to generate ICE candidates and provide
// an object that implements the PeerConnectionObserver interface.
// 3. Create local MediaStream and MediaTracks using the PeerConnectionFactory
// and add it to PeerConnection by calling AddStream.
// 4. Create an offer and serialize it and send it to the remote peer.
// 5. Once an ice candidate have been found PeerConnection will call the
// observer function OnIceCandidate. The candidates must also be serialized and
// sent to the remote peer.
// 6. Once an answer is received from the remote peer, call
// SetLocalSessionDescription with the offer and SetRemoteSessionDescription
// with the remote answer.
// 7. Once a remote candidate is received from the remote peer, provide it to
// the peerconnection by calling AddIceCandidate.


// The Receiver of a call can decide to accept or reject the call.
// This decision will be taken by the application not peerconnection.
// If application decides to accept the call
// 1. Create PeerConnectionFactoryInterface if it doesn't exist.
// 2. Create a new PeerConnection.
// 3. Provide the remote offer to the new PeerConnection object by calling
// SetRemoteSessionDescription.
// 4. Generate an answer to the remote offer by calling CreateAnswer and send it
// back to the remote peer.
// 5. Provide the local answer to the new PeerConnection by calling
// SetLocalSessionDescription with the answer.
// 6. Provide the remote ice candidates by calling AddIceCandidate.
// 7. Once a candidate have been found PeerConnection will call the observer
// function OnIceCandidate. Send these candidates to the remote peer.

#ifndef TALK_APP_WEBRTC_PEERCONNECTIONINTERFACE_H_
#define TALK_APP_WEBRTC_PEERCONNECTIONINTERFACE_H_

#include <string>
#include <vector>

#include "app/webrtc/datachannelinterface.h"
#include "app/webrtc/dtmfsenderinterface.h"
#include "app/webrtc/jsep.h"
#include "app/webrtc/statstypes.h"
#include "app/webrtc/mediastreaminterface.h"
#include "base/socketaddress.h"

namespace talk_base {
class Thread;
}

namespace cricket {
class PortAllocator;
}

namespace webrtc {
class AudioDeviceModule;

// MediaStream container interface.
class StreamCollectionInterface : public talk_base::RefCountInterface {
 public:
  // TODO(ronghuawu): Update the function names to c++ style, e.g. find -> Find.
  virtual size_t count() = 0;
  virtual MediaStreamInterface* at(size_t index) = 0;
  virtual MediaStreamInterface* find(const std::string& label) = 0;
  virtual MediaStreamTrackInterface* FindAudioTrack(
      const std::string& id) = 0;
  virtual MediaStreamTrackInterface* FindVideoTrack(
      const std::string& id) = 0;

 protected:
  // Dtor protected as objects shouldn't be deleted via this interface.
  ~StreamCollectionInterface() {}
};

class StatsObserver : public talk_base::RefCountInterface {
 public:
  virtual void OnComplete(const std::vector<StatsReport>& reports) = 0;

 protected:
  virtual ~StatsObserver() {}
};

class PeerConnectionInterface : public talk_base::RefCountInterface {
 public:
  // See http://dev.w3.org/2011/webrtc/editor/webrtc.html#state-definitions .
  enum SignalingState {
    kStable,
    kHaveLocalOffer,
    kHaveLocalPrAnswer,
    kHaveRemoteOffer,
    kHaveRemotePrAnswer,
    kClosed,
  };

  // TODO(bemasc): Remove IceState when callers are changed to
  // IceConnection/GatheringState.
  enum IceState {
    kIceNew,
    kIceGathering,
    kIceWaiting,
    kIceChecking,
    kIceConnected,
    kIceCompleted,
    kIceFailed,
    kIceClosed,
  };

  enum IceGatheringState {
    kIceGatheringNew,
    kIceGatheringGathering,
    kIceGatheringComplete
  };

  enum IceConnectionState {
    kIceConnectionNew,
    kIceConnectionChecking,
    kIceConnectionConnected,
    kIceConnectionCompleted,
    kIceConnectionFailed,
    kIceConnectionDisconnected,
    kIceConnectionClosed,
  };

  struct IceServer {
    std::string uri;
    std::string password;
  };
  typedef std::vector<IceServer> IceServers;

  // Accessor methods to active local streams.
  virtual talk_base::scoped_refptr<StreamCollectionInterface>
      local_streams() = 0;

  // Accessor methods to remote streams.
  virtual talk_base::scoped_refptr<StreamCollectionInterface>
      remote_streams() = 0;

  // Add a new MediaStream to be sent on this PeerConnection.
  // Note that a SessionDescription negotiation is needed before the
  // remote peer can receive the stream.
  virtual bool AddStream(MediaStreamInterface* stream,
                         const MediaConstraintsInterface* constraints) = 0;

  // Remove a MediaStream from this PeerConnection.
  // Note that a SessionDescription negotiation is need before the
  // remote peer is notified.
  virtual void RemoveStream(MediaStreamInterface* stream) = 0;

  // Returns pointer to the created DtmfSender on success.
  // Otherwise returns NULL.
  virtual talk_base::scoped_refptr<DtmfSenderInterface> CreateDtmfSender(
      AudioTrackInterface* track) = 0;

  virtual bool GetStats(StatsObserver* observer,
                        MediaStreamTrackInterface* track) = 0;

  virtual talk_base::scoped_refptr<DataChannelInterface> CreateDataChannel(
      const std::string& label,
      const DataChannelInit* config) = 0;

  virtual const SessionDescriptionInterface* local_description() const = 0;
  virtual const SessionDescriptionInterface* remote_description() const = 0;

  // Create a new offer.
  // The CreateSessionDescriptionObserver callback will be called when done.
  virtual void CreateOffer(CreateSessionDescriptionObserver* observer,
                           const MediaConstraintsInterface* constraints) = 0;
  // Create an answer to an offer.
  // The CreateSessionDescriptionObserver callback will be called when done.
  virtual void CreateAnswer(CreateSessionDescriptionObserver* observer,
                            const MediaConstraintsInterface* constraints) = 0;
  // Sets the local session description.
  // JsepInterface takes the ownership of |desc| even if it fails.
  // The |observer| callback will be called when done.
  virtual void SetLocalDescription(SetSessionDescriptionObserver* observer,
                                   SessionDescriptionInterface* desc) = 0;
  // Sets the remote session description.
  // JsepInterface takes the ownership of |desc| even if it fails.
  // The |observer| callback will be called when done.
  virtual void SetRemoteDescription(SetSessionDescriptionObserver* observer,
                                    SessionDescriptionInterface* desc) = 0;
  // Restarts or updates the ICE Agent process of gathering local candidates
  // and pinging remote candidates.
  virtual bool UpdateIce(const IceServers& configuration,
                         const MediaConstraintsInterface* constraints) = 0;
  // Provides a remote candidate to the ICE Agent.
  // A copy of the |candidate| will be created and added to the remote
  // description. So the caller of this method still has the ownership of the
  // |candidate|.
  // TODO(ronghuawu): Consider to change this so that the AddIceCandidate will
  // take the ownership of the |candidate|.
  virtual bool AddIceCandidate(const IceCandidateInterface* candidate) = 0;

  // Deprecated, please use SignalingState instead.
  // TODO(perkj): Remove ready_state when callers are changed.
  typedef SignalingState ReadyState;
  virtual ReadyState ready_state() = 0;

  // Returns the current SignalingState.
  virtual SignalingState signaling_state() = 0;

  // TODO(bemasc): Remove ice_state when callers are changed to
  // IceConnection/GatheringState.
  // Returns the current IceState.
  virtual IceState ice_state() = 0;
  virtual IceConnectionState ice_connection_state() = 0;
  virtual IceGatheringState ice_gathering_state() = 0;

 protected:
  // Dtor protected as objects shouldn't be deleted via this interface.
  ~PeerConnectionInterface() {}
};

// PeerConnection callback interface. Application should implement these
// methods.
class PeerConnectionObserver {
 public:
  enum StateType {
    kSignalingState,
    kIceState,
  };

  virtual void OnError() = 0;

  // Triggered when the SignalingState changed.
  virtual void OnSignalingChange(
     PeerConnectionInterface::SignalingState new_state) {}

  // Triggered when SignalingState or IceState have changed.
  // TODO(bemasc): Remove once callers transition to OnSignalingChange.
  virtual void OnStateChange(StateType state_changed) {}

  // Triggered when media is received on a new stream from remote peer.
  virtual void OnAddStream(MediaStreamInterface* stream) = 0;

  // Triggered when a remote peer close a stream.
  virtual void OnRemoveStream(MediaStreamInterface* stream) = 0;

  // Triggered when a remote peer open a data channel.
  // TODO(perkj): Make pure virtual.
  virtual void OnDataChannel(DataChannelInterface* data_channel) {}

  // Triggered when renegotation is needed, for example the ICE has restarted.
  virtual void OnRenegotiationNeeded() {}

  // Called any time the IceConnectionState changes
  virtual void OnIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) {}

  // Called any time the IceGatheringState changes
  virtual void OnIceGatheringChange(
      PeerConnectionInterface::IceGatheringState new_state) {}

  // New Ice candidate have been found.
  virtual void OnIceCandidate(const IceCandidateInterface* candidate) = 0;

  // TODO(bemasc): Remove this once callers transition to OnIceGatheringChange.
  // All Ice candidates have been found.
  virtual void OnIceComplete() {}

 protected:
  // Dtor protected as objects shouldn't be deleted via this interface.
  ~PeerConnectionObserver() {}
};

// Factory class used for creating cricket::PortAllocator that is used
// for ICE negotiation.
class PortAllocatorFactoryInterface : public talk_base::RefCountInterface {
 public:
  struct StunConfiguration {
    StunConfiguration(const std::string& address, int port)
        : server(address, port) {}
    // STUN server address and port.
    talk_base::SocketAddress server;
  };

  struct TurnConfiguration {
    TurnConfiguration(const std::string& address,
                      int port,
                      const std::string& username,
                      const std::string& password)
        : server(address, port),
          username(username),
          password(password) {}
    talk_base::SocketAddress server;
    std::string username;
    std::string password;
  };

  virtual cricket::PortAllocator* CreatePortAllocator(
      const std::vector<StunConfiguration>& stun_servers,
      const std::vector<TurnConfiguration>& turn_configurations) = 0;

 protected:
  PortAllocatorFactoryInterface() {}
  ~PortAllocatorFactoryInterface() {}
};

// PeerConnectionFactoryInterface is the factory interface use for creating
// PeerConnection, MediaStream and media tracks.
// PeerConnectionFactoryInterface will create required libjingle threads,
// socket and network manager factory classes for networking.
// If an application decides to provide its own threads and network
// implementation of these classes it should use the alternate
// CreatePeerConnectionFactory method which accepts threads as input and use the
// CreatePeerConnection version that takes a PortAllocatorFactoryInterface as
// argument.
class PeerConnectionFactoryInterface : public talk_base::RefCountInterface {
 public:
  virtual talk_base::scoped_refptr<PeerConnectionInterface>
     CreatePeerConnection(
         const PeerConnectionInterface::IceServers& configuration,
         const MediaConstraintsInterface* constraints,
         PeerConnectionObserver* observer) = 0;
  virtual talk_base::scoped_refptr<PeerConnectionInterface>
      CreatePeerConnection(
          const PeerConnectionInterface::IceServers& configuration,
          const MediaConstraintsInterface* constraints,
          PortAllocatorFactoryInterface* allocator_factory,
          PeerConnectionObserver* observer) = 0;
  virtual talk_base::scoped_refptr<LocalMediaStreamInterface>
      CreateLocalMediaStream(const std::string& label) = 0;

  // Creates a AudioSourceInterface.
  // |constraints| decides audio processing settings but can be NULL.
  virtual talk_base::scoped_refptr<AudioSourceInterface> CreateAudioSource(
      const MediaConstraintsInterface* constraints) = 0;

  // Creates a VideoSourceInterface. The new source take ownership of
  // |capturer|. |constraints| decides video resolution and frame rate but can
  // be NULL.
  virtual talk_base::scoped_refptr<VideoSourceInterface> CreateVideoSource(
      cricket::VideoCapturer* capturer,
      const MediaConstraintsInterface* constraints) = 0;

  // Creates a new local VideoTrack. The same |source| can be used in several
  // tracks.
  virtual talk_base::scoped_refptr<VideoTrackInterface>
      CreateVideoTrack(const std::string& label,
                       VideoSourceInterface* source) = 0;

  // Creates an new AudioTrack. At the moment |source| can be NULL.
  virtual talk_base::scoped_refptr<AudioTrackInterface>
      CreateAudioTrack(const std::string& label,
                       AudioSourceInterface* source) = 0;

  // Deprecated: Please use the version that take a source as input.
  virtual talk_base::scoped_refptr<LocalVideoTrackInterface>
      CreateLocalVideoTrack(const std::string& label,
                            cricket::VideoCapturer* video_device) = 0;

  // Deprecated: Please use the version that take a source as input.
  virtual talk_base::scoped_refptr<LocalAudioTrackInterface>
      CreateLocalAudioTrack(const std::string& label,
                            AudioDeviceModule* audio_device) = 0;

 protected:
  // Dtor and ctor protected as objects shouldn't be created or deleted via
  // this interface.
  PeerConnectionFactoryInterface() {}
  ~PeerConnectionFactoryInterface() {} // NOLINT
};

// Create a new instance of PeerConnectionFactoryInterface.
talk_base::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory();

// Create a new instance of PeerConnectionFactoryInterface.
// Ownership of |factory| and |default_adm| is transferred to the returned
// factory.
talk_base::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory(talk_base::Thread* worker_thread,
                            talk_base::Thread* signaling_thread,
                            AudioDeviceModule* default_adm);

// TODO(perkj): The JsepInterface has been combined with
// PeerConnectionInterface. Remove the below interface once no clients use
// JsepInterface::IceServer.
class JsepInterface {
 public:
  typedef PeerConnectionInterface::IceServer IceServer;
  typedef std::vector<PeerConnectionInterface::IceServer> IceServers;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_PEERCONNECTIONINTERFACE_H_
