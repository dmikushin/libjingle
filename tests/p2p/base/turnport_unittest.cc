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

#include "base/basicpacketsocketfactory.h"
#include "base/buffer.h"
#include "base/logging.h"
#include "base/gunit.h"
#include "base/helpers.h"
#include "base/physicalsocketserver.h"
#include "base/scoped_ptr.h"
#include "base/socketaddress.h"
#include "base/thread.h"
#include "base/virtualsocketserver.h"
#include "p2p/base/constants.h"
#include "p2p/base/testturnserver.h"
#include "p2p/base/turnport.h"
#include "p2p/base/udpport.h"

using talk_base::SocketAddress;
using cricket::Connection;
using cricket::Port;
using cricket::PortInterface;
using cricket::TurnPort;
using cricket::UDPPort;

static const SocketAddress kLocalAddr1("11.11.11.11", 0);
static const SocketAddress kLocalAddr2("22.22.22.22", 0);
static const SocketAddress kTurnUdpIntAddr("99.99.99.4",
                                           cricket::TURN_SERVER_PORT);
static const SocketAddress kTurnUdpExtAddr("99.99.99.5", 0);

static const char kIceUfrag1[] = "TESTICEUFRAG0001";
static const char kIceUfrag2[] = "TESTICEUFRAG0002";
static const char kIcePwd1[] = "TESTICEPWD00000000000001";
static const char kIcePwd2[] = "TESTICEPWD00000000000002";
static const char kTurnUsername[] = "test";
static const char kTurnPassword[] = "test";
static const int kTimeout = 1000;

class TurnPortTest : public testing::Test,
                     public sigslot::has_slots<> {
 public:
  TurnPortTest()
      : main_(talk_base::Thread::Current()),
        pss_(new talk_base::PhysicalSocketServer),
        ss_(new talk_base::VirtualSocketServer(pss_.get())),
        ss_scope_(ss_.get()),
        network_("unittest", "unittest", talk_base::IPAddress(INADDR_ANY), 32),
        socket_factory_(talk_base::Thread::Current()),
        turn_server_(main_, kTurnUdpIntAddr, kTurnUdpExtAddr),
        turn_ready_(false),
        turn_error_(false),
        turn_unknown_address_(false),
        udp_ready_(false) {
    network_.AddIP(talk_base::IPAddress(INADDR_ANY));
  }

  void OnTurnAddressReady(Port* port) {
    turn_ready_ = true;
  }
  void OnTurnAddressError(Port* port) {
    turn_error_ = true;
  }
  void OnTurnUnknownAddress(PortInterface* port, const SocketAddress& addr,
                            cricket::ProtocolType proto,
                            cricket::IceMessage* msg, const std::string& rf,
                            bool /*port_muxed*/) {
    turn_unknown_address_ = true;
  }
  void OnTurnReadPacket(Connection* conn, const char* data, size_t size) {
    turn_packets_.push_back(talk_base::Buffer(data, size));
  }
  void OnUdpAddressReady(Port* port) {
    udp_ready_ = true;
  }
  void OnUdpReadPacket(Connection* conn, const char* data, size_t size) {
    udp_packets_.push_back(talk_base::Buffer(data, size));
  }


  void CreateTurnPort(const SocketAddress& server_address,
                      const std::string& username,
                      const std::string& password) {
    cricket::RelayCredentials credentials(username, password);
    turn_port_.reset(TurnPort::Create(main_, &socket_factory_, &network_,
                                 kLocalAddr1.ipaddr(), 0, 0,
                                 kIceUfrag1, kIcePwd1,
                                 server_address, credentials));
    turn_port_->SignalAddressReady.connect(this,
        &TurnPortTest::OnTurnAddressReady);
    turn_port_->SignalAddressError.connect(this,
        &TurnPortTest::OnTurnAddressError);
    turn_port_->SignalUnknownAddress.connect(this,
        &TurnPortTest::OnTurnUnknownAddress);
  }
  void CreateUdpPort() {
    udp_port_.reset(UDPPort::Create(main_, &socket_factory_, &network_,
                                    kLocalAddr2.ipaddr(), 0, 0,
                                    kIceUfrag2, kIcePwd2));
    udp_port_->SignalAddressReady.connect(
        this, &TurnPortTest::OnUdpAddressReady);
  }

 protected:
  talk_base::Thread* main_;
  talk_base::scoped_ptr<talk_base::PhysicalSocketServer> pss_;
  talk_base::scoped_ptr<talk_base::VirtualSocketServer> ss_;
  talk_base::SocketServerScope ss_scope_;
  talk_base::Network network_;
  talk_base::BasicPacketSocketFactory socket_factory_;
  cricket::TestTurnServer turn_server_;
  talk_base::scoped_ptr<TurnPort> turn_port_;
  talk_base::scoped_ptr<UDPPort> udp_port_;
  bool turn_ready_;
  bool turn_error_;
  bool turn_unknown_address_;
  bool udp_ready_;
  std::vector<talk_base::Buffer> turn_packets_;
  std::vector<talk_base::Buffer> udp_packets_;
};

// Do a normal TURN allocation.
TEST_F(TurnPortTest, TestTurnAllocate) {
  CreateTurnPort(kTurnUdpIntAddr, kTurnUsername, kTurnPassword);
  turn_port_->PrepareAddress();
  EXPECT_TRUE_WAIT(turn_ready_, kTimeout);
  ASSERT_EQ(1U, turn_port_->Candidates().size());
  EXPECT_EQ(kTurnUdpExtAddr.ipaddr(),
            turn_port_->Candidates()[0].address().ipaddr());
  EXPECT_NE(0, turn_port_->Candidates()[0].address().port());
}

// Try to do a TURN allocation with an invalid password.
TEST_F(TurnPortTest, TestTurnAllocateBadPassword) {
  CreateTurnPort(kTurnUdpIntAddr, kTurnUsername, "bad");
  turn_port_->PrepareAddress();
  EXPECT_TRUE_WAIT(turn_error_, kTimeout);
  ASSERT_EQ(0U, turn_port_->Candidates().size());
}

// Do a TURN allocation and try to send a packet to it from the outside.
// The packet should be dropped. Then, try to send a packet from TURN to the
// outside. It should reach its destination. Finally, try again from the
// outside. It should now work as well.
TEST_F(TurnPortTest, TestTurnConnection) {
  // Create ports and prepare addresses.
  CreateTurnPort(kTurnUdpIntAddr, kTurnUsername, kTurnPassword);
  turn_port_->PrepareAddress();
  ASSERT_TRUE_WAIT(turn_ready_, kTimeout);
  CreateUdpPort();
  udp_port_->PrepareAddress();
  ASSERT_TRUE_WAIT(udp_ready_, kTimeout);

  // Send ping from UDP to TURN.
  Connection* conn1 = udp_port_->CreateConnection(
      turn_port_->Candidates()[0], Port::ORIGIN_MESSAGE);
  ASSERT_TRUE(conn1 != NULL);
  conn1->Ping(0);
  WAIT(!turn_unknown_address_, kTimeout);
  EXPECT_FALSE(turn_unknown_address_);
  EXPECT_EQ(Connection::STATE_READ_INIT, conn1->read_state());
  EXPECT_EQ(Connection::STATE_WRITE_INIT, conn1->write_state());

  // Send ping from TURN to UDP.
  Connection* conn2 = turn_port_->CreateConnection(
      udp_port_->Candidates()[0], Port::ORIGIN_MESSAGE);
  ASSERT_TRUE(conn2 != NULL);
  conn2->Ping(0);
  EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, conn2->write_state(), kTimeout);
  EXPECT_EQ(Connection::STATE_READABLE, conn1->read_state());
  EXPECT_EQ(Connection::STATE_READ_INIT, conn2->read_state());
  EXPECT_EQ(Connection::STATE_WRITE_INIT, conn1->write_state());

  // Send another ping from UDP to TURN.
  conn1->Ping(0);
  EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, conn1->write_state(), kTimeout);
  EXPECT_EQ(Connection::STATE_READABLE, conn2->read_state());
}

// Do a TURN allocation, establish a connection, and send some data.
TEST_F(TurnPortTest, TestTurnSendData) {
  // Create ports and prepare addresses.
  CreateTurnPort(kTurnUdpIntAddr, kTurnUsername, kTurnPassword);
  turn_port_->PrepareAddress();
  EXPECT_TRUE_WAIT(turn_ready_, kTimeout);
  CreateUdpPort();
  udp_port_->PrepareAddress();
  EXPECT_TRUE_WAIT(udp_ready_, kTimeout);

  // Create connections and send pings.
  Connection* conn1 = turn_port_->CreateConnection(
      udp_port_->Candidates()[0], Port::ORIGIN_MESSAGE);
  Connection* conn2 = udp_port_->CreateConnection(
      turn_port_->Candidates()[0], Port::ORIGIN_MESSAGE);
  ASSERT_TRUE(conn1 != NULL);
  ASSERT_TRUE(conn2 != NULL);
  conn1->SignalReadPacket.connect(static_cast<TurnPortTest*>(this),
                                  &TurnPortTest::OnTurnReadPacket);
  conn2->SignalReadPacket.connect(static_cast<TurnPortTest*>(this),
                                  &TurnPortTest::OnUdpReadPacket);
  conn1->Ping(0);
  EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, conn1->write_state(), kTimeout);
  conn2->Ping(0);
  EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, conn2->write_state(), kTimeout);

  // Send some data.
  size_t num_packets = 256;
  for (size_t i = 0; i < num_packets; ++i) {
    char buf[256];
    for (size_t j = 0; j < i + 1; ++j) {
      buf[j] = 0xFF - j;
    }
    conn1->Send(buf, i + 1);
    conn2->Send(buf, i + 1);
    main_->ProcessMessages(0);
  }

  // Check the data.
  ASSERT_EQ_WAIT(num_packets, turn_packets_.size(), kTimeout);
  ASSERT_EQ_WAIT(num_packets, udp_packets_.size(), kTimeout);
  for (size_t i = 0; i < num_packets; ++i) {
    EXPECT_EQ(i + 1, turn_packets_[i].length());
    EXPECT_EQ(i + 1, udp_packets_[i].length());
    EXPECT_EQ(turn_packets_[i], udp_packets_[i]);
  }
}
