/*
 * libjingle
 * Copyright 2011 Google Inc.
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

#include "base/fakesslidentity.h"
#include "base/gunit.h"
#include "base/thread.h"
#include "p2p/base/constants.h"
#include "p2p/base/fakesession.h"
#include "p2p/base/parsing.h"
#include "p2p/base/p2ptransport.h"
#include "p2p/base/rawtransport.h"
#include "p2p/base/sessionmessages.h"
#include "xmlelement.h"
#include "constants.h"

using cricket::Candidate;
using cricket::Candidates;
using cricket::Transport;
using cricket::FakeTransport;
using cricket::TransportChannel;
using cricket::FakeTransportChannel;
using cricket::TransportRole;
using cricket::TransportDescription;
using cricket::WriteError;
using cricket::ParseError;
using talk_base::SocketAddress;

class TransportTest : public testing::Test,
                      public sigslot::has_slots<> {
 public:
  TransportTest()
      : thread_(talk_base::Thread::Current()),
        transport_(new FakeTransport(
            thread_, thread_, "test content name", NULL)),
        channel_(NULL),
        connecting_signalled_(false) {
    transport_->SignalConnecting.connect(this, &TransportTest::OnConnecting);
  }
  ~TransportTest() {
    transport_->DestroyAllChannels();
  }
  bool SetupChannel() {
    channel_ = CreateChannel(1);
    return (channel_ != NULL);
  }
  FakeTransportChannel* CreateChannel(int component) {
    return static_cast<FakeTransportChannel*>(
        transport_->CreateChannel(component));
  }

 protected:
  void OnConnecting(Transport* transport) {
    connecting_signalled_ = true;
  }

  talk_base::Thread* thread_;
  talk_base::scoped_ptr<FakeTransport> transport_;
  FakeTransportChannel* channel_;
  bool connecting_signalled_;
};

class FakeCandidateTranslator : public cricket::CandidateTranslator {
 public:
  void AddMapping(int component, const std::string& channel_name) {
    name_to_component[channel_name] = component;
    component_to_name[component] = channel_name;
  }

  bool GetChannelNameFromComponent(
      int component, std::string* channel_name) const {
    if (component_to_name.find(component) == component_to_name.end()) {
      return false;
    }
    *channel_name = component_to_name.find(component)->second;
    return true;
  }
  bool GetComponentFromChannelName(
      const std::string& channel_name, int* component) const {
    if (name_to_component.find(channel_name) == name_to_component.end()) {
      return false;
    }
    *component = name_to_component.find(channel_name)->second;
    return true;
  }

  std::map<std::string, int> name_to_component;
  std::map<int, std::string> component_to_name;
};

// Test that calling ConnectChannels triggers an OnConnecting signal.
TEST_F(TransportTest, TestConnectChannelsDoesSignal) {
  EXPECT_TRUE(SetupChannel());
  transport_->ConnectChannels();
  EXPECT_FALSE(connecting_signalled_);

  EXPECT_TRUE_WAIT(connecting_signalled_, 100);
}

// Test that DestroyAllChannels kills any pending OnConnecting signals.
TEST_F(TransportTest, TestDestroyAllClearsPosts) {
  EXPECT_TRUE(transport_->CreateChannel(1) != NULL);

  transport_->ConnectChannels();
  transport_->DestroyAllChannels();

  thread_->ProcessMessages(0);
  EXPECT_FALSE(connecting_signalled_);
}

// Tests that SetRole and SetTiebreaker are properly passed down to channels.
TEST_F(TransportTest, TestSetRoleAndTiebreaker) {
  transport_->SetRole(cricket::ROLE_CONTROLLED);
  transport_->SetTiebreaker(99U);
  EXPECT_TRUE(SetupChannel());
  EXPECT_EQ(cricket::ROLE_CONTROLLED, channel_->role());
  EXPECT_EQ(99U, channel_->tiebreaker());
  transport_->SetRole(cricket::ROLE_CONTROLLING);
  EXPECT_EQ(cricket::ROLE_CONTROLLING, channel_->role());
}

// Tests that we can properly serialize/deserialize candidates.
TEST_F(TransportTest, TestP2PTransportWriteAndParseCandidate) {
  Candidate test_candidate(
      "", 1, "udp",
      talk_base::SocketAddress("2001:db8:fefe::1", 9999),
      738197504, "abcdef", "ghijkl", "foo", "testnet", 50, "");
  Candidate test_candidate2(
      "", 2, "tcp",
      talk_base::SocketAddress("192.168.7.1", 9999),
      1107296256, "mnopqr", "stuvwx", "bar", "testnet2", 100, "");
  talk_base::SocketAddress host_address("www.google.com", 24601);
  host_address.SetResolvedIP(talk_base::IPAddress(0x0A000001));
  Candidate test_candidate3(
      "", 3, "spdy", host_address, 1476395008, "yzabcd",
      "efghij", "baz", "testnet3", 150, "");
  WriteError write_error;
  ParseError parse_error;
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  cricket::Candidate parsed_candidate;
  cricket::P2PTransportParser parser;

  FakeCandidateTranslator translator;
  translator.AddMapping(1, "test");
  translator.AddMapping(2, "test2");
  translator.AddMapping(3, "test3");

  EXPECT_TRUE(parser.WriteGingleCandidate(test_candidate, &translator,
                                          elem.accept(), &write_error));
  EXPECT_EQ("", write_error.text);
  EXPECT_EQ("test", elem->Attr(buzz::QN_NAME));
  EXPECT_EQ("udp", elem->Attr(cricket::QN_PROTOCOL));
  EXPECT_EQ("2001:db8:fefe::1", elem->Attr(cricket::QN_ADDRESS));
  EXPECT_EQ("9999", elem->Attr(cricket::QN_PORT));
  EXPECT_EQ("0.34", elem->Attr(cricket::QN_PREFERENCE));
  EXPECT_EQ("abcdef", elem->Attr(cricket::QN_USERNAME));
  EXPECT_EQ("ghijkl", elem->Attr(cricket::QN_PASSWORD));
  EXPECT_EQ("foo", elem->Attr(cricket::QN_TYPE));
  EXPECT_EQ("testnet", elem->Attr(cricket::QN_NETWORK));
  EXPECT_EQ("50", elem->Attr(cricket::QN_GENERATION));

  EXPECT_TRUE(parser.ParseGingleCandidate(elem.get(), &translator,
                                          &parsed_candidate, &parse_error));
  EXPECT_TRUE(test_candidate.IsEquivalent(parsed_candidate));

  EXPECT_TRUE(parser.WriteGingleCandidate(test_candidate2, &translator,
                                          elem.accept(), &write_error));
  EXPECT_EQ("test2", elem->Attr(buzz::QN_NAME));
  EXPECT_EQ("tcp", elem->Attr(cricket::QN_PROTOCOL));
  EXPECT_EQ("192.168.7.1", elem->Attr(cricket::QN_ADDRESS));
  EXPECT_EQ("9999", elem->Attr(cricket::QN_PORT));
  EXPECT_EQ("0.51", elem->Attr(cricket::QN_PREFERENCE));
  EXPECT_EQ("mnopqr", elem->Attr(cricket::QN_USERNAME));
  EXPECT_EQ("stuvwx", elem->Attr(cricket::QN_PASSWORD));
  EXPECT_EQ("bar", elem->Attr(cricket::QN_TYPE));
  EXPECT_EQ("testnet2", elem->Attr(cricket::QN_NETWORK));
  EXPECT_EQ("100", elem->Attr(cricket::QN_GENERATION));

  EXPECT_TRUE(parser.ParseGingleCandidate(elem.get(), &translator,
                                          &parsed_candidate, &parse_error));
  EXPECT_TRUE(test_candidate2.IsEquivalent(parsed_candidate));

  // Check that an ip is preferred over hostname.
  EXPECT_TRUE(parser.WriteGingleCandidate(test_candidate3, &translator,
                                          elem.accept(), &write_error));
  EXPECT_EQ("test3", elem->Attr(cricket::QN_NAME));
  EXPECT_EQ("spdy", elem->Attr(cricket::QN_PROTOCOL));
  EXPECT_EQ("10.0.0.1", elem->Attr(cricket::QN_ADDRESS));
  EXPECT_EQ("24601", elem->Attr(cricket::QN_PORT));
  EXPECT_EQ("0.69", elem->Attr(cricket::QN_PREFERENCE));
  EXPECT_EQ("yzabcd", elem->Attr(cricket::QN_USERNAME));
  EXPECT_EQ("efghij", elem->Attr(cricket::QN_PASSWORD));
  EXPECT_EQ("baz", elem->Attr(cricket::QN_TYPE));
  EXPECT_EQ("testnet3", elem->Attr(cricket::QN_NETWORK));
  EXPECT_EQ("150", elem->Attr(cricket::QN_GENERATION));

  EXPECT_TRUE(parser.ParseGingleCandidate(elem.get(), &translator,
                                          &parsed_candidate, &parse_error));
  EXPECT_TRUE(test_candidate3.IsEquivalent(parsed_candidate));
}
