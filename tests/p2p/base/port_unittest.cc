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

#include "base/basicpacketsocketfactory.h"
#include "base/crc32.h"
#include "base/gunit.h"
#include "base/helpers.h"
#include "base/host.h"
#include "base/logging.h"
#include "base/natserver.h"
#include "base/natsocketfactory.h"
#include "base/physicalsocketserver.h"
#include "base/scoped_ptr.h"
#include "base/socketaddress.h"
#include "base/stringutils.h"
#include "base/thread.h"
#include "base/virtualsocketserver.h"
#include "p2p/base/portproxy.h"
#include "p2p/base/relayport.h"
#include "p2p/base/stunport.h"
#include "p2p/base/tcpport.h"
#include "p2p/base/testrelayserver.h"
#include "p2p/base/teststunserver.h"
#include "p2p/base/testturnserver.h"
#include "p2p/base/transport.h"
#include "p2p/base/turnport.h"

using talk_base::AsyncPacketSocket;
using talk_base::ByteBuffer;
using talk_base::NATType;
using talk_base::NAT_OPEN_CONE;
using talk_base::NAT_ADDR_RESTRICTED;
using talk_base::NAT_PORT_RESTRICTED;
using talk_base::NAT_SYMMETRIC;
using talk_base::PacketSocketFactory;
using talk_base::scoped_ptr;
using talk_base::Socket;
using talk_base::SocketAddress;
using namespace cricket;

static const int kTimeout = 1000;
static const SocketAddress kLocalAddr1("192.168.1.2", 0);
static const SocketAddress kLocalAddr2("192.168.1.3", 0);
static const SocketAddress kNatAddr1("77.77.77.77", talk_base::NAT_SERVER_PORT);
static const SocketAddress kNatAddr2("88.88.88.88", talk_base::NAT_SERVER_PORT);
static const SocketAddress kStunAddr("99.99.99.1", STUN_SERVER_PORT);
static const SocketAddress kRelayUdpIntAddr("99.99.99.2", 5000);
static const SocketAddress kRelayUdpExtAddr("99.99.99.3", 5001);
static const SocketAddress kRelayTcpIntAddr("99.99.99.2", 5002);
static const SocketAddress kRelayTcpExtAddr("99.99.99.3", 5003);
static const SocketAddress kRelaySslTcpIntAddr("99.99.99.2", 5004);
static const SocketAddress kRelaySslTcpExtAddr("99.99.99.3", 5005);
static const SocketAddress kTurnUdpIntAddr("99.99.99.4", STUN_SERVER_PORT);
static const SocketAddress kTurnUdpExtAddr("99.99.99.5", 0);
static const RelayCredentials kRelayCredentials("test", "test");

// TODO: Update these when RFC5245 is completely supported.
// Magic value of 30 is from RFC3484, for IPv4 addresses.
static const uint32 kDefaultPrflxPriority = ICE_TYPE_PREFERENCE_PRFLX << 24 |
             30 << 8 | (256 - ICE_CANDIDATE_COMPONENT_DEFAULT);
static const int STUN_ERROR_BAD_REQUEST_AS_GICE =
    STUN_ERROR_BAD_REQUEST / 256 * 100 + STUN_ERROR_BAD_REQUEST % 256;
static const int STUN_ERROR_UNAUTHORIZED_AS_GICE =
    STUN_ERROR_UNAUTHORIZED / 256 * 100 + STUN_ERROR_UNAUTHORIZED % 256;
static const int STUN_ERROR_SERVER_ERROR_AS_GICE =
    STUN_ERROR_SERVER_ERROR / 256 * 100 + STUN_ERROR_SERVER_ERROR % 256;

static const int kTiebreaker1 = 11111;
static const int kTiebreaker2 = 22222;

static Candidate GetCandidate(Port* port) {
  assert(port->Candidates().size() == 1);
  return port->Candidates()[0];
}

static SocketAddress GetAddress(Port* port) {
  return GetCandidate(port).address();
}

static IceMessage* CopyStunMessage(const IceMessage* src) {
  IceMessage* dst = new IceMessage();
  ByteBuffer buf;
  src->Write(&buf);
  dst->Read(&buf);
  return dst;
}

static bool WriteStunMessage(const StunMessage* msg, ByteBuffer* buf) {
  buf->Resize(0);  // clear out any existing buffer contents
  return msg->Write(buf);
}

// Stub port class for testing STUN generation and processing.
class TestPort : public Port {
 public:
  TestPort(talk_base::Thread* thread, const std::string& type,
           talk_base::PacketSocketFactory* factory, talk_base::Network* network,
           const talk_base::IPAddress& ip, int min_port, int max_port,
           const std::string& username_fragment, const std::string& password)
      : Port(thread, type, ICE_TYPE_PREFERENCE_HOST, factory, network, ip,
             min_port, max_port, username_fragment, password) {
  }
  ~TestPort() {}

  // Expose GetStunMessage so that we can test it.
  using cricket::Port::GetStunMessage;

  // The last StunMessage that was sent on this Port.
  // TODO: Make these const; requires changes to SendXXXXResponse.
  ByteBuffer* last_stun_buf() { return last_stun_buf_.get(); }
  IceMessage* last_stun_msg() { return last_stun_msg_.get(); }
  int last_stun_error_code() {
    int code = 0;
    if (last_stun_msg_) {
      const StunErrorCodeAttribute* error_attr = last_stun_msg_->GetErrorCode();
      if (error_attr) {
        code = error_attr->code();
      }
    }
    return code;
  }

  virtual void PrepareAddress() {
    talk_base::SocketAddress addr(ip(), min_port());
    AddAddress(addr, addr, "udp", Type(), type_preference(), true);
  }

  // Exposed for testing candidate building.
  void AddCandidateAddress(const talk_base::SocketAddress& addr) {
    AddAddress(addr, addr, "udp", Type(), type_preference(), false);
  }
  void AddCandidateAddress(const talk_base::SocketAddress& addr,
                           const talk_base::SocketAddress& base_address,
                           const std::string& type,
                           bool final) {
    AddAddress(addr, base_address, "udp", type, type_preference(), final);
  }

  virtual Connection* CreateConnection(const Candidate& remote_candidate,
                                       CandidateOrigin origin) {
    Connection* conn = new ProxyConnection(this, 0, remote_candidate);
    AddConnection(conn);
    return conn;
  }
  virtual int SendTo(
      const void* data, size_t size, const talk_base::SocketAddress& addr,
      bool payload) {
    if (!payload) {
      IceMessage* msg = new IceMessage;
      ByteBuffer* buf = new ByteBuffer(static_cast<const char*>(data), size);
      ByteBuffer::ReadPosition pos(buf->GetReadPosition());
      if (!msg->Read(buf)) {
        delete msg;
        delete buf;
        return -1;
      }
      buf->SetReadPosition(pos);
      last_stun_buf_.reset(buf);
      last_stun_msg_.reset(msg);
    }
    return size;
  }
  virtual int SetOption(talk_base::Socket::Option opt, int value) {
    return 0;
  }
  virtual int GetOption(talk_base::Socket::Option opt, int* value) {
    return -1;
  }
  virtual int GetError() {
    return 0;
  }
  void Reset() {
    last_stun_buf_.reset();
    last_stun_msg_.reset();
  }

 private:
  talk_base::scoped_ptr<ByteBuffer> last_stun_buf_;
  talk_base::scoped_ptr<IceMessage> last_stun_msg_;
};

class TestChannel : public sigslot::has_slots<> {
 public:
  TestChannel(Port* p1, Port* p2)
      : src_(p1), dst_(p2), address_count_(0), conn_(NULL),
        remote_request_(NULL) {
    src_->SignalAddressReady.connect(this, &TestChannel::OnPortReady);
    src_->SignalUnknownAddress.connect(this, &TestChannel::OnUnknownAddress);
  }

  int address_count() { return address_count_; }
  Connection* conn() { return conn_; }
  const SocketAddress& remote_address() { return remote_address_; }
  const std::string remote_fragment() { return remote_frag_; }

  void Start() {
    src_->PrepareAddress();
  }
  void CreateConnection() {
    conn_ = src_->CreateConnection(GetCandidate(dst_), Port::ORIGIN_MESSAGE);
  }
  void AcceptConnection() {
    ASSERT_TRUE(remote_request_.get() != NULL);
    Candidate c = GetCandidate(dst_);
    c.set_address(remote_address_);
    conn_ = src_->CreateConnection(c, Port::ORIGIN_MESSAGE);
    src_->SendBindingResponse(remote_request_.get(), remote_address_);
    remote_request_.reset();
  }
  void Ping() {
    Ping(0);
  }
  void Ping(uint32 now) {
    conn_->Ping(now);
  }
  void Stop() {
    conn_->SignalDestroyed.connect(this, &TestChannel::OnDestroyed);
    conn_->Destroy();
  }

  void OnPortReady(Port* port) {
    address_count_++;
  }

  void OnUnknownAddress(PortInterface* port, const SocketAddress& addr,
                        ProtocolType proto,
                        IceMessage* msg, const std::string& rf,
                        bool /*port_muxed*/) {
    ASSERT_EQ(src_.get(), port);
    if (!remote_address_.IsNil()) {
      ASSERT_EQ(remote_address_, addr);
    }
    // MI and PRIORITY attribute should be present in ping requests when port
    // is in ICEPROTO_RFC5245 mode.
    const cricket::StunUInt32Attribute* priority_attr =
        msg->GetUInt32(STUN_ATTR_PRIORITY);
    const cricket::StunByteStringAttribute* mi_attr =
        msg->GetByteString(STUN_ATTR_MESSAGE_INTEGRITY);
    const cricket::StunUInt32Attribute* fingerprint_attr =
        msg->GetUInt32(STUN_ATTR_FINGERPRINT);
    if (src_->IceProtocol() == cricket::ICEPROTO_RFC5245) {
      EXPECT_TRUE(priority_attr != NULL);
      EXPECT_TRUE(mi_attr != NULL);
      EXPECT_TRUE(fingerprint_attr != NULL);
    } else {
      EXPECT_TRUE(priority_attr == NULL);
      EXPECT_TRUE(mi_attr == NULL);
      EXPECT_TRUE(fingerprint_attr == NULL);
    }
    remote_address_ = addr;
    remote_request_.reset(CopyStunMessage(msg));
    remote_frag_ = rf;
  }

  void OnDestroyed(Connection* conn) {
    ASSERT_EQ(conn_, conn);
    conn_ = NULL;
  }

 private:
  talk_base::Thread* thread_;
  talk_base::scoped_ptr<Port> src_;
  Port* dst_;

  int address_count_;
  Connection* conn_;
  SocketAddress remote_address_;
  talk_base::scoped_ptr<StunMessage> remote_request_;
  std::string remote_frag_;
};

class PortTest : public testing::Test, public sigslot::has_slots<> {
 public:
  PortTest()
      : main_(talk_base::Thread::Current()),
        pss_(new talk_base::PhysicalSocketServer),
        ss_(new talk_base::VirtualSocketServer(pss_.get())),
        ss_scope_(ss_.get()),
        network_("unittest", "unittest", talk_base::IPAddress(INADDR_ANY), 32),
        socket_factory_(talk_base::Thread::Current()),
        nat_factory1_(ss_.get(), kNatAddr1),
        nat_factory2_(ss_.get(), kNatAddr2),
        nat_socket_factory1_(&nat_factory1_),
        nat_socket_factory2_(&nat_factory2_),
        stun_server_(main_, kStunAddr),
        turn_server_(main_, kTurnUdpIntAddr, kTurnUdpExtAddr),
        relay_server_(main_, kRelayUdpIntAddr, kRelayUdpExtAddr,
                      kRelayTcpIntAddr, kRelayTcpExtAddr,
                      kRelaySslTcpIntAddr, kRelaySslTcpExtAddr),
        username_(talk_base::CreateRandomString(ICE_UFRAG_LENGTH)),
        password_(talk_base::CreateRandomString(ICE_PWD_LENGTH)),
        ice_protocol_(cricket::ICEPROTO_GOOGLE),
        role_conflict_(false) {
    network_.AddIP(talk_base::IPAddress(INADDR_ANY));
  }

 protected:
  static void SetUpTestCase() {
    // Ensure the RNG is inited.
    talk_base::InitRandom(NULL, 0);
  }

  void TestLocalToLocal() {
    Port* port1 = CreateUdpPort(kLocalAddr1);
    Port* port2 = CreateUdpPort(kLocalAddr2);
    TestConnectivity("udp", port1, "udp", port2, true, true, true, true);
  }
  void TestLocalToStun(NATType ntype) {
    Port* port1 = CreateUdpPort(kLocalAddr1);
    nat_server2_.reset(CreateNatServer(kNatAddr2, ntype));
    Port* port2 = CreateStunPort(kLocalAddr2, &nat_socket_factory2_);
    TestConnectivity("udp", port1, StunName(ntype), port2,
                     ntype == NAT_OPEN_CONE, true,
                     ntype != NAT_SYMMETRIC, true);
  }
  void TestLocalToRelay(RelayType rtype, ProtocolType proto) {
    Port* port1 = CreateUdpPort(kLocalAddr1);
    Port* port2 = CreateRelayPort(kLocalAddr2, rtype, proto, PROTO_UDP);
    TestConnectivity("udp", port1, RelayName(rtype, proto), port2,
                     rtype == RELAY_GTURN, true, true, true);
  }
  void TestStunToLocal(NATType ntype) {
    nat_server1_.reset(CreateNatServer(kNatAddr1, ntype));
    Port* port1 = CreateStunPort(kLocalAddr1, &nat_socket_factory1_);
    Port* port2 = CreateUdpPort(kLocalAddr2);
    TestConnectivity(StunName(ntype), port1, "udp", port2,
                     true, ntype != NAT_SYMMETRIC, true, true);
  }
  void TestStunToStun(NATType ntype1, NATType ntype2) {
    nat_server1_.reset(CreateNatServer(kNatAddr1, ntype1));
    Port* port1 = CreateStunPort(kLocalAddr1, &nat_socket_factory1_);
    nat_server2_.reset(CreateNatServer(kNatAddr2, ntype2));
    Port* port2 = CreateStunPort(kLocalAddr2, &nat_socket_factory2_);
    TestConnectivity(StunName(ntype1), port1, StunName(ntype2), port2,
                     ntype2 == NAT_OPEN_CONE,
                     ntype1 != NAT_SYMMETRIC, ntype2 != NAT_SYMMETRIC,
                     ntype1 + ntype2 < (NAT_PORT_RESTRICTED + NAT_SYMMETRIC));
  }
  void TestStunToRelay(NATType ntype, RelayType rtype, ProtocolType proto) {
    nat_server1_.reset(CreateNatServer(kNatAddr1, ntype));
    Port* port1 = CreateStunPort(kLocalAddr1, &nat_socket_factory1_);
    Port* port2 = CreateRelayPort(kLocalAddr2, rtype, proto, PROTO_UDP);
    TestConnectivity(StunName(ntype), port1, RelayName(rtype, proto), port2,
                     rtype == RELAY_GTURN, ntype != NAT_SYMMETRIC, true, true);
  }
  void TestTcpToTcp() {
    Port* port1 = CreateTcpPort(kLocalAddr1);
    Port* port2 = CreateTcpPort(kLocalAddr2);
    TestConnectivity("tcp", port1, "tcp", port2, true, false, true, true);
  }
  void TestTcpToRelay(RelayType rtype, ProtocolType proto) {
    Port* port1 = CreateTcpPort(kLocalAddr1);
    Port* port2 = CreateRelayPort(kLocalAddr2, rtype, proto, PROTO_TCP);
    TestConnectivity("tcp", port1, RelayName(rtype, proto), port2,
                     rtype == RELAY_GTURN, false, true, true);
  }
  void TestSslTcpToRelay(RelayType rtype, ProtocolType proto) {
    Port* port1 = CreateTcpPort(kLocalAddr1);
    Port* port2 = CreateRelayPort(kLocalAddr2, rtype, proto, PROTO_SSLTCP);
    TestConnectivity("ssltcp", port1, RelayName(rtype, proto), port2,
                     rtype == RELAY_GTURN, false, true, true);
  }

  // helpers for above functions
  UDPPort* CreateUdpPort(const SocketAddress& addr) {
    return CreateUdpPort(addr, &socket_factory_);
  }
  UDPPort* CreateUdpPort(const SocketAddress& addr,
                         PacketSocketFactory* socket_factory) {
    UDPPort* port = UDPPort::Create(main_, socket_factory, &network_,
                                    addr.ipaddr(), 0, 0, username_, password_);
    port->SetIceProtocolType(ice_protocol_);
    return port;
  }
  TCPPort* CreateTcpPort(const SocketAddress& addr) {
    TCPPort* port = CreateTcpPort(addr, &socket_factory_);
    port->SetIceProtocolType(ice_protocol_);
    return port;
  }
  TCPPort* CreateTcpPort(const SocketAddress& addr,
                        PacketSocketFactory* socket_factory) {
    TCPPort* port = TCPPort::Create(main_, socket_factory, &network_,
                                    addr.ipaddr(), 0, 0, username_, password_,
                                    true);
    port->SetIceProtocolType(ice_protocol_);
    return port;
  }
  StunPort* CreateStunPort(const SocketAddress& addr,
                           talk_base::PacketSocketFactory* factory) {
    StunPort* port = StunPort::Create(main_, factory, &network_,
                                      addr.ipaddr(), 0, 0,
                                      username_, password_, kStunAddr);
    port->SetIceProtocolType(ice_protocol_);
    return port;
  }
  Port* CreateRelayPort(const SocketAddress& addr, RelayType rtype,
                        ProtocolType int_proto, ProtocolType ext_proto) {
    if (rtype == RELAY_TURN) {
      return CreateTurnPort(addr, int_proto, ext_proto);
    } else {
      return CreateGturnPort(addr, int_proto, ext_proto);
    }
  }
  TurnPort* CreateTurnPort(const SocketAddress& addr,
                           ProtocolType int_proto, ProtocolType ext_proto) {
    TurnPort* port = TurnPort::Create(main_, &socket_factory_, &network_,
                                      addr.ipaddr(), 0, 0,
                                      username_, password_, kTurnUdpIntAddr,
                                      kRelayCredentials);
    port->SetIceProtocolType(ice_protocol_);
    return port;
  }
  RelayPort* CreateGturnPort(const SocketAddress& addr,
                             ProtocolType int_proto, ProtocolType ext_proto) {
    RelayPort* port = CreateGturnPort(addr);
    SocketAddress addrs[] =
        { kRelayUdpIntAddr, kRelayTcpIntAddr, kRelaySslTcpIntAddr };
    port->AddServerAddress(ProtocolAddress(addrs[int_proto], int_proto));
    return port;
  }
  RelayPort* CreateGturnPort(const SocketAddress& addr) {
    RelayPort* port = RelayPort::Create(main_, &socket_factory_, &network_,
                                        addr.ipaddr(), 0, 0,
                                        username_, password_);
    // TODO: Add an external address for ext_proto, so that the
    // other side can connect to this port using a non-UDP protocol.
    port->SetIceProtocolType(ice_protocol_);
    return port;
  }
  talk_base::NATServer* CreateNatServer(const SocketAddress& addr,
                                        talk_base::NATType type) {
    return new talk_base::NATServer(type, ss_.get(), addr, ss_.get(), addr);
  }
  static const char* StunName(NATType type) {
    switch (type) {
      case NAT_OPEN_CONE:       return "stun(open cone)";
      case NAT_ADDR_RESTRICTED: return "stun(addr restricted)";
      case NAT_PORT_RESTRICTED: return "stun(port restricted)";
      case NAT_SYMMETRIC:       return "stun(symmetric)";
      default:                  return "stun(?)";
    }
  }
  static const char* RelayName(RelayType type, ProtocolType proto) {
    if (type == RELAY_TURN) {
      switch (proto) {
        case PROTO_UDP:           return "turn(udp)";
        case PROTO_TCP:           return "turn(tcp)";
        case PROTO_SSLTCP:        return "turn(ssltcp)";
        default:                  return "turn(?)";
      }
    } else {
      switch (proto) {
        case PROTO_UDP:           return "gturn(udp)";
        case PROTO_TCP:           return "gturn(tcp)";
        case PROTO_SSLTCP:        return "gturn(ssltcp)";
        default:                  return "gturn(?)";
      }
    }
  }

  void TestCrossFamilyPorts(int type);

  // this does all the work
  void TestConnectivity(const char* name1, Port* port1,
                        const char* name2, Port* port2,
                        bool accept, bool same_addr1,
                        bool same_addr2, bool possible);

  void SetIceProtocolType(cricket::IceProtocolType protocol) {
    ice_protocol_ = protocol;
  }

  IceMessage* CreateStunMessage(int type) {
    IceMessage* msg = new IceMessage();
    msg->SetType(type);
    msg->SetTransactionID("TESTTESTTEST");
    return msg;
  }
  IceMessage* CreateStunMessageWithUsername(int type,
                                            const std::string& username) {
    IceMessage* msg = CreateStunMessage(type);
    msg->AddAttribute(
        new StunByteStringAttribute(STUN_ATTR_USERNAME, username));
    return msg;
  }
  TestPort* CreateTestPort(const talk_base::SocketAddress& addr,
                           const std::string& username,
                           const std::string& password) {
    TestPort* port =  new TestPort(main_, "test", &socket_factory_, &network_,
                                   addr.ipaddr(), 0, 0, username, password);
    port->SignalRoleConflict.connect(this, &PortTest::OnRoleConflict);
    return port;
  }

  void OnRoleConflict() {
    role_conflict_ = true;
  }

  bool role_conflict() const { return role_conflict_; }

 private:
  talk_base::Thread* main_;
  talk_base::scoped_ptr<talk_base::PhysicalSocketServer> pss_;
  talk_base::scoped_ptr<talk_base::VirtualSocketServer> ss_;
  talk_base::SocketServerScope ss_scope_;
  talk_base::Network network_;
  talk_base::BasicPacketSocketFactory socket_factory_;
  talk_base::scoped_ptr<talk_base::NATServer> nat_server1_;
  talk_base::scoped_ptr<talk_base::NATServer> nat_server2_;
  talk_base::NATSocketFactory nat_factory1_;
  talk_base::NATSocketFactory nat_factory2_;
  talk_base::BasicPacketSocketFactory nat_socket_factory1_;
  talk_base::BasicPacketSocketFactory nat_socket_factory2_;
  TestStunServer stun_server_;
  TestTurnServer turn_server_;
  TestRelayServer relay_server_;
  std::string username_;
  std::string password_;
  cricket::IceProtocolType ice_protocol_;
  bool role_conflict_;
};

void PortTest::TestConnectivity(const char* name1, Port* port1,
                                const char* name2, Port* port2,
                                bool accept, bool same_addr1,
                                bool same_addr2, bool possible) {
  LOG(LS_INFO) << "Test: " << name1 << " to " << name2 << ": ";
  port1->set_component(cricket::ICE_CANDIDATE_COMPONENT_DEFAULT);
  port2->set_component(cricket::ICE_CANDIDATE_COMPONENT_DEFAULT);

  // Set up channels.
  TestChannel ch1(port1, port2);
  TestChannel ch2(port2, port1);
  EXPECT_EQ(0, ch1.address_count());
  EXPECT_EQ(0, ch2.address_count());

  // Acquire addresses.
  ch1.Start();
  ch2.Start();
  ASSERT_EQ_WAIT(1, ch1.address_count(), kTimeout);
  ASSERT_EQ_WAIT(1, ch2.address_count(), kTimeout);

  // Send a ping from src to dst. This may or may not make it.
  ch1.CreateConnection();
  ASSERT_TRUE(ch1.conn() != NULL);
  EXPECT_TRUE_WAIT(ch1.conn()->connected(), kTimeout);  // for TCP connect
  ch1.Ping();
  WAIT(!ch2.remote_address().IsNil(), kTimeout);

  if (accept) {
    // We are able to send a ping from src to dst. This is the case when
    // sending to UDP ports and cone NATs.
    EXPECT_TRUE(ch1.remote_address().IsNil());
    EXPECT_EQ(ch2.remote_fragment(), port1->username_fragment());

    // Ensure the ping came from the same address used for src.
    // This is the case unless the source NAT was symmetric.
    if (same_addr1) EXPECT_EQ(ch2.remote_address(), GetAddress(port1));
    EXPECT_TRUE(same_addr2);

    // Send a ping from dst to src.
    ch2.AcceptConnection();
    ASSERT_TRUE(ch2.conn() != NULL);
    ch2.Ping();
    EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, ch2.conn()->write_state(),
                   kTimeout);
  } else {
    // We can't send a ping from src to dst, so flip it around. This will happen
    // when the destination NAT is addr/port restricted or symmetric.
    EXPECT_TRUE(ch1.remote_address().IsNil());
    EXPECT_TRUE(ch2.remote_address().IsNil());

    // Send a ping from dst to src. Again, this may or may not make it.
    ch2.CreateConnection();
    ASSERT_TRUE(ch2.conn() != NULL);
    ch2.Ping();
    WAIT(ch2.conn()->write_state() == Connection::STATE_WRITABLE, kTimeout);

    if (same_addr1 && same_addr2) {
      // The new ping got back to the source.
      EXPECT_EQ(Connection::STATE_READABLE, ch1.conn()->read_state());
      EXPECT_EQ(Connection::STATE_WRITABLE, ch2.conn()->write_state());

      // First connection may not be writable if the first ping did not get
      // through.  So we will have to do another.
      if (ch1.conn()->write_state() == Connection::STATE_WRITE_INIT) {
        ch1.Ping();
        EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, ch1.conn()->write_state(),
                       kTimeout);
      }
    } else if (!same_addr1 && possible) {
      // The new ping went to the candidate address, but that address was bad.
      // This will happen when the source NAT is symmetric.
      EXPECT_TRUE(ch1.remote_address().IsNil());
      EXPECT_TRUE(ch2.remote_address().IsNil());

      // However, since we have now sent a ping to the source IP, we should be
      // able to get a ping from it. This gives us the real source address.
      ch1.Ping();
      EXPECT_TRUE_WAIT(!ch2.remote_address().IsNil(), kTimeout);
      EXPECT_EQ(Connection::STATE_READ_INIT, ch2.conn()->read_state());
      EXPECT_TRUE(ch1.remote_address().IsNil());

      // Pick up the actual address and establish the connection.
      ch2.AcceptConnection();
      ASSERT_TRUE(ch2.conn() != NULL);
      ch2.Ping();
      EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, ch2.conn()->write_state(),
                     kTimeout);
    } else if (!same_addr2 && possible) {
      // The new ping came in, but from an unexpected address. This will happen
      // when the destination NAT is symmetric.
      EXPECT_FALSE(ch1.remote_address().IsNil());
      EXPECT_EQ(Connection::STATE_READ_INIT, ch1.conn()->read_state());

      // Update our address and complete the connection.
      ch1.AcceptConnection();
      ch1.Ping();
      EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, ch1.conn()->write_state(),
                     kTimeout);
    } else {  // (!possible)
      // There should be s no way for the pings to reach each other. Check it.
      EXPECT_TRUE(ch1.remote_address().IsNil());
      EXPECT_TRUE(ch2.remote_address().IsNil());
      ch1.Ping();
      WAIT(!ch2.remote_address().IsNil(), kTimeout);
      EXPECT_TRUE(ch1.remote_address().IsNil());
      EXPECT_TRUE(ch2.remote_address().IsNil());
    }
  }

  // Everything should be good, unless we know the situation is impossible.
  ASSERT_TRUE(ch1.conn() != NULL);
  ASSERT_TRUE(ch2.conn() != NULL);
  if (possible) {
    EXPECT_EQ(Connection::STATE_READABLE, ch1.conn()->read_state());
    EXPECT_EQ(Connection::STATE_WRITABLE, ch1.conn()->write_state());
    EXPECT_EQ(Connection::STATE_READABLE, ch2.conn()->read_state());
    EXPECT_EQ(Connection::STATE_WRITABLE, ch2.conn()->write_state());
  } else {
    EXPECT_NE(Connection::STATE_READABLE, ch1.conn()->read_state());
    EXPECT_NE(Connection::STATE_WRITABLE, ch1.conn()->write_state());
    EXPECT_NE(Connection::STATE_READABLE, ch2.conn()->read_state());
    EXPECT_NE(Connection::STATE_WRITABLE, ch2.conn()->write_state());
  }

  // Tear down and ensure that goes smoothly.
  ch1.Stop();
  ch2.Stop();
  EXPECT_TRUE_WAIT(ch1.conn() == NULL, kTimeout);
  EXPECT_TRUE_WAIT(ch2.conn() == NULL, kTimeout);
}

class FakePacketSocketFactory : public talk_base::PacketSocketFactory {
 public:
  FakePacketSocketFactory()
      : next_udp_socket_(NULL),
        next_server_tcp_socket_(NULL),
        next_client_tcp_socket_(NULL) {
  }
  virtual ~FakePacketSocketFactory() { }

  virtual AsyncPacketSocket* CreateUdpSocket(
      const SocketAddress& address, int min_port, int max_port) {
    EXPECT_TRUE(next_udp_socket_ != NULL);
    AsyncPacketSocket* result = next_udp_socket_;
    next_udp_socket_ = NULL;
    return result;
  }

  virtual AsyncPacketSocket* CreateServerTcpSocket(
      const SocketAddress& local_address, int min_port, int max_port,
      bool ssl) {
    EXPECT_TRUE(next_server_tcp_socket_ != NULL);
    AsyncPacketSocket* result = next_server_tcp_socket_;
    next_server_tcp_socket_ = NULL;
    return result;
  }

  // TODO: |proxy_info| and |user_agent| should be set
  // per-factory and not when socket is created.
  virtual AsyncPacketSocket* CreateClientTcpSocket(
      const SocketAddress& local_address, const SocketAddress& remote_address,
      const talk_base::ProxyInfo& proxy_info,
      const std::string& user_agent, bool ssl) {
    EXPECT_TRUE(next_client_tcp_socket_ != NULL);
    AsyncPacketSocket* result = next_client_tcp_socket_;
    next_client_tcp_socket_ = NULL;
    return result;
  }

  void set_next_udp_socket(AsyncPacketSocket* next_udp_socket) {
    next_udp_socket_ = next_udp_socket;
  }
  void set_next_server_tcp_socket(AsyncPacketSocket* next_server_tcp_socket) {
    next_server_tcp_socket_ = next_server_tcp_socket;
  }
  void set_next_client_tcp_socket(AsyncPacketSocket* next_client_tcp_socket) {
    next_client_tcp_socket_ = next_client_tcp_socket;
  }

 private:
  AsyncPacketSocket* next_udp_socket_;
  AsyncPacketSocket* next_server_tcp_socket_;
  AsyncPacketSocket* next_client_tcp_socket_;
};

class FakeAsyncPacketSocket : public AsyncPacketSocket {
 public:
  // Returns current local address. Address may be set to NULL if the
  // socket is not bound yet (GetState() returns STATE_BINDING).
  virtual SocketAddress GetLocalAddress() const {
    return SocketAddress();
  }

  // Returns remote address. Returns zeroes if this is not a client TCP socket.
  virtual SocketAddress GetRemoteAddress() const {
    return SocketAddress();
  }

  // Send a packet.
  virtual int Send(const void *pv, size_t cb) {
    return cb;
  }
  virtual int SendTo(const void *pv, size_t cb, const SocketAddress& addr) {
    return cb;
  }
  virtual int Close() {
    return 0;
  }

  virtual State GetState() const { return state_; }
  virtual int GetOption(Socket::Option opt, int* value) { return 0; }
  virtual int SetOption(Socket::Option opt, int value) { return 0; }
  virtual int GetError() const { return 0; }
  virtual void SetError(int error) { }

  void set_state(State state) { state_ = state; }

 private:
  State state_;
};

// Local -> XXXX
TEST_F(PortTest, TestLocalToLocal) {
  TestLocalToLocal();
}

TEST_F(PortTest, TestLocalToConeNat) {
  TestLocalToStun(NAT_OPEN_CONE);
}

TEST_F(PortTest, TestLocalToARNat) {
  TestLocalToStun(NAT_ADDR_RESTRICTED);
}

TEST_F(PortTest, TestLocalToPRNat) {
  TestLocalToStun(NAT_PORT_RESTRICTED);
}

TEST_F(PortTest, TestLocalToSymNat) {
  TestLocalToStun(NAT_SYMMETRIC);
}

TEST_F(PortTest, TestLocalToTurn) {
  TestLocalToRelay(RELAY_TURN, PROTO_UDP);
}

TEST_F(PortTest, TestLocalToGturn) {
  TestLocalToRelay(RELAY_GTURN, PROTO_UDP);
}

TEST_F(PortTest, TestLocalToTcpGturn) {
  TestLocalToRelay(RELAY_GTURN, PROTO_TCP);
}

TEST_F(PortTest, TestLocalToSslTcpGturn) {
  TestLocalToRelay(RELAY_GTURN, PROTO_SSLTCP);
}

// Cone NAT -> XXXX
TEST_F(PortTest, TestConeNatToLocal) {
  TestStunToLocal(NAT_OPEN_CONE);
}

TEST_F(PortTest, TestConeNatToConeNat) {
  TestStunToStun(NAT_OPEN_CONE, NAT_OPEN_CONE);
}

TEST_F(PortTest, TestConeNatToARNat) {
  TestStunToStun(NAT_OPEN_CONE, NAT_ADDR_RESTRICTED);
}

TEST_F(PortTest, TestConeNatToPRNat) {
  TestStunToStun(NAT_OPEN_CONE, NAT_PORT_RESTRICTED);
}

TEST_F(PortTest, TestConeNatToSymNat) {
  TestStunToStun(NAT_OPEN_CONE, NAT_SYMMETRIC);
}

TEST_F(PortTest, TestConeNatToTurn) {
  TestStunToRelay(NAT_OPEN_CONE, RELAY_TURN, PROTO_UDP);
}

TEST_F(PortTest, TestConeNatToGturn) {
  TestStunToRelay(NAT_OPEN_CONE, RELAY_GTURN, PROTO_UDP);
}

TEST_F(PortTest, TestConeNatToTcpGturn) {
  TestStunToRelay(NAT_OPEN_CONE, RELAY_GTURN, PROTO_TCP);
}

// Address-restricted NAT -> XXXX
TEST_F(PortTest, TestARNatToLocal) {
  TestStunToLocal(NAT_ADDR_RESTRICTED);
}

TEST_F(PortTest, TestARNatToConeNat) {
  TestStunToStun(NAT_ADDR_RESTRICTED, NAT_OPEN_CONE);
}

TEST_F(PortTest, TestARNatToARNat) {
  TestStunToStun(NAT_ADDR_RESTRICTED, NAT_ADDR_RESTRICTED);
}

TEST_F(PortTest, TestARNatToPRNat) {
  TestStunToStun(NAT_ADDR_RESTRICTED, NAT_PORT_RESTRICTED);
}

TEST_F(PortTest, TestARNatToSymNat) {
  TestStunToStun(NAT_ADDR_RESTRICTED, NAT_SYMMETRIC);
}

TEST_F(PortTest, TestARNatToTurn) {
  TestStunToRelay(NAT_ADDR_RESTRICTED, RELAY_TURN, PROTO_UDP);
}

TEST_F(PortTest, TestARNatToGturn) {
  TestStunToRelay(NAT_ADDR_RESTRICTED, RELAY_GTURN, PROTO_UDP);
}

TEST_F(PortTest, TestARNATNatToTcpGturn) {
  TestStunToRelay(NAT_ADDR_RESTRICTED, RELAY_GTURN, PROTO_TCP);
}

// Port-restricted NAT -> XXXX
TEST_F(PortTest, TestPRNatToLocal) {
  TestStunToLocal(NAT_PORT_RESTRICTED);
}

TEST_F(PortTest, TestPRNatToConeNat) {
  TestStunToStun(NAT_PORT_RESTRICTED, NAT_OPEN_CONE);
}

TEST_F(PortTest, TestPRNatToARNat) {
  TestStunToStun(NAT_PORT_RESTRICTED, NAT_ADDR_RESTRICTED);
}

TEST_F(PortTest, TestPRNatToPRNat) {
  TestStunToStun(NAT_PORT_RESTRICTED, NAT_PORT_RESTRICTED);
}

TEST_F(PortTest, TestPRNatToSymNat) {
  // Will "fail"
  TestStunToStun(NAT_PORT_RESTRICTED, NAT_SYMMETRIC);
}

TEST_F(PortTest, TestPRNatToTurn) {
  TestStunToRelay(NAT_PORT_RESTRICTED, RELAY_TURN, PROTO_UDP);
}

TEST_F(PortTest, TestPRNatToGturn) {
  TestStunToRelay(NAT_PORT_RESTRICTED, RELAY_GTURN, PROTO_UDP);
}

TEST_F(PortTest, TestPRNatToTcpGturn) {
  TestStunToRelay(NAT_PORT_RESTRICTED, RELAY_GTURN, PROTO_TCP);
}

// Symmetric NAT -> XXXX
TEST_F(PortTest, TestSymNatToLocal) {
  TestStunToLocal(NAT_SYMMETRIC);
}

TEST_F(PortTest, TestSymNatToConeNat) {
  TestStunToStun(NAT_SYMMETRIC, NAT_OPEN_CONE);
}

TEST_F(PortTest, TestSymNatToARNat) {
  TestStunToStun(NAT_SYMMETRIC, NAT_ADDR_RESTRICTED);
}

TEST_F(PortTest, TestSymNatToPRNat) {
  // Will "fail"
  TestStunToStun(NAT_SYMMETRIC, NAT_PORT_RESTRICTED);
}

TEST_F(PortTest, TestSymNatToSymNat) {
  // Will "fail"
  TestStunToStun(NAT_SYMMETRIC, NAT_SYMMETRIC);
}

TEST_F(PortTest, TestSymNatToTurn) {
  TestStunToRelay(NAT_SYMMETRIC, RELAY_TURN, PROTO_UDP);
}

TEST_F(PortTest, TestSymNatToGturn) {
  TestStunToRelay(NAT_SYMMETRIC, RELAY_GTURN, PROTO_UDP);
}

TEST_F(PortTest, TestSymNatToTcpGturn) {
  TestStunToRelay(NAT_SYMMETRIC, RELAY_GTURN, PROTO_TCP);
}

// Outbound TCP -> XXXX
TEST_F(PortTest, TestTcpToTcp) {
  TestTcpToTcp();
}

/* TODO: Enable these once testrelayserver can accept external TCP.
TEST_F(PortTest, TestTcpToTcpRelay) {
  TestTcpToRelay(PROTO_TCP);
}

TEST_F(PortTest, TestTcpToSslTcpRelay) {
  TestTcpToRelay(PROTO_SSLTCP);
}
*/

// Outbound SSLTCP -> XXXX
/* TODO: Enable these once testrelayserver can accept external SSL.
TEST_F(PortTest, TestSslTcpToTcpRelay) {
  TestSslTcpToRelay(PROTO_TCP);
}

TEST_F(PortTest, TestSslTcpToSslTcpRelay) {
  TestSslTcpToRelay(PROTO_SSLTCP);
}
*/

// This test case verifies standard ICE features in STUN messages. Currently it
// verifies Message Integrity attribute in STUN messages and username in STUN
// binding request will have colon (":") between remote and local username.
TEST_F(PortTest, TestLocalToLocalAsIce) {
  SetIceProtocolType(cricket::ICEPROTO_RFC5245);
  UDPPort* port1 = CreateUdpPort(kLocalAddr1);
  port1->SetRole(cricket::ROLE_CONTROLLING);
  port1->SetTiebreaker(kTiebreaker1);
  ASSERT_EQ(cricket::ICEPROTO_RFC5245, port1->IceProtocol());
  UDPPort* port2 = CreateUdpPort(kLocalAddr2);
  port2->SetRole(cricket::ROLE_CONTROLLED);
  port2->SetTiebreaker(kTiebreaker2);
  ASSERT_EQ(cricket::ICEPROTO_RFC5245, port2->IceProtocol());
  // Same parameters as TestLocalToLocal above.
  TestConnectivity("udp", port1, "udp", port2, true, true, true, true);
}

// This test is trying to validate a successful and failure scenario in a
// loopback test when protocol is RFC5245. For success tiebreaker, username
// should remain equal to the request generated by the port and role of port
// must be in controlling.
TEST_F(PortTest, TestLoopbackCallAsIce) {
  talk_base::scoped_ptr<TestPort> lport(
      CreateTestPort(kLocalAddr1, "lfrag", "lpass"));
  lport->SetIceProtocolType(ICEPROTO_RFC5245);
  lport->SetRole(cricket::ROLE_CONTROLLING);
  lport->SetTiebreaker(kTiebreaker1);
  lport->PrepareAddress();
  ASSERT_FALSE(lport->Candidates().empty());
  Connection* conn = lport->CreateConnection(lport->Candidates()[0],
                                             Port::ORIGIN_MESSAGE);
  conn->Ping(0);

  ASSERT_TRUE_WAIT(lport->last_stun_msg() != NULL, 1000);
  IceMessage* msg = lport->last_stun_msg();
  EXPECT_EQ(STUN_BINDING_REQUEST, msg->type());
  conn->OnReadPacket(lport->last_stun_buf()->Data(),
                     lport->last_stun_buf()->Length());
  ASSERT_TRUE_WAIT(lport->last_stun_msg() != NULL, 1000);
  msg = lport->last_stun_msg();
  EXPECT_EQ(STUN_BINDING_RESPONSE, msg->type());

  // If the tiebreaker value is different from port, we expect a error response.
  lport->Reset();
  lport->AddCandidateAddress(kLocalAddr2);
  // Creating a different connection as |conn| is in STATE_READABLE.
  Connection* conn1 = lport->CreateConnection(lport->Candidates()[1],
                                              Port::ORIGIN_MESSAGE);
  conn1->Ping(0);

  ASSERT_TRUE_WAIT(lport->last_stun_msg() != NULL, 1000);
  msg = lport->last_stun_msg();
  EXPECT_EQ(STUN_BINDING_REQUEST, msg->type());
  IceMessage* modified_req = CreateStunMessage(STUN_BINDING_REQUEST);
  const StunByteStringAttribute* username_attr = msg->GetByteString(
      STUN_ATTR_USERNAME);
  modified_req->AddAttribute(new StunByteStringAttribute(
      STUN_ATTR_USERNAME, username_attr->GetString()));
  // To make sure we receive error response, adding tiebreaker less than
  // what's present in request.
  modified_req->AddAttribute(new StunUInt64Attribute(
      STUN_ATTR_ICE_CONTROLLING, kTiebreaker1 - 1));
  modified_req->AddMessageIntegrity("lpass");
  modified_req->AddFingerprint();

  lport->Reset();
  talk_base::scoped_ptr<ByteBuffer> buf(new ByteBuffer());
  WriteStunMessage(modified_req, buf.get());
  conn1->OnReadPacket(buf->Data(), buf->Length());
  ASSERT_TRUE_WAIT(lport->last_stun_msg() != NULL, 1000);
  msg = lport->last_stun_msg();
  EXPECT_EQ(STUN_BINDING_ERROR_RESPONSE, msg->type());
}

// This test verifies role conflict signal is received when there is
// conflict in the role. In this case both ports are in controlling and
// |rport| has higher tiebreaker value than |lport|. Since |lport| has lower
// value of tiebreaker, when it receives ping request from |rport| it will
// send role conflict signal.
TEST_F(PortTest, TestIceRoleConflict) {
  talk_base::scoped_ptr<TestPort> lport(
      CreateTestPort(kLocalAddr1, "lfrag", "lpass"));
  lport->SetIceProtocolType(ICEPROTO_RFC5245);
  lport->SetRole(cricket::ROLE_CONTROLLING);
  lport->SetTiebreaker(kTiebreaker1);
  talk_base::scoped_ptr<TestPort> rport(
      CreateTestPort(kLocalAddr2, "rfrag", "rpass"));
  rport->SetIceProtocolType(ICEPROTO_RFC5245);
  rport->SetRole(cricket::ROLE_CONTROLLING);
  rport->SetTiebreaker(kTiebreaker2);

  lport->PrepareAddress();
  rport->PrepareAddress();
  ASSERT_FALSE(lport->Candidates().empty());
  ASSERT_FALSE(rport->Candidates().empty());
  Connection* lconn = lport->CreateConnection(rport->Candidates()[0],
                                              Port::ORIGIN_MESSAGE);
  Connection* rconn = rport->CreateConnection(lport->Candidates()[0],
                                              Port::ORIGIN_MESSAGE);
  rconn->Ping(0);

  ASSERT_TRUE_WAIT(rport->last_stun_msg() != NULL, 1000);
  IceMessage* msg = rport->last_stun_msg();
  EXPECT_EQ(STUN_BINDING_REQUEST, msg->type());
  // Send rport binding request to lport.
  lconn->OnReadPacket(rport->last_stun_buf()->Data(),
                      rport->last_stun_buf()->Length());

  ASSERT_TRUE_WAIT(lport->last_stun_msg() != NULL, 1000);
  EXPECT_EQ(STUN_BINDING_RESPONSE, lport->last_stun_msg()->type());
  EXPECT_TRUE(role_conflict());
}

TEST_F(PortTest, TestTcpNoDelay) {
  TCPPort* port1 = CreateTcpPort(kLocalAddr1);
  int option_value = -1;
  int success = port1->GetOption(talk_base::Socket::OPT_NODELAY,
                                 &option_value);
  ASSERT_EQ(0, success);  // GetOption() should complete successfully w/ 0
  ASSERT_EQ(1, option_value);
  delete port1;
}

TEST_F(PortTest, TestDelayedBindingUdp) {
  FakeAsyncPacketSocket *socket = new FakeAsyncPacketSocket();
  FakePacketSocketFactory socket_factory;

  socket_factory.set_next_udp_socket(socket);
  scoped_ptr<UDPPort> port(
      CreateUdpPort(kLocalAddr1, &socket_factory));

  socket->set_state(AsyncPacketSocket::STATE_BINDING);
  port->PrepareAddress();

  EXPECT_EQ(0U, port->Candidates().size());
  socket->SignalAddressReady(socket, kLocalAddr2);

  EXPECT_EQ(1U, port->Candidates().size());
}

TEST_F(PortTest, TestDelayedBindingTcp) {
  FakeAsyncPacketSocket *socket = new FakeAsyncPacketSocket();
  FakePacketSocketFactory socket_factory;

  socket_factory.set_next_server_tcp_socket(socket);
  scoped_ptr<TCPPort> port(
      CreateTcpPort(kLocalAddr1, &socket_factory));

  socket->set_state(AsyncPacketSocket::STATE_BINDING);
  port->PrepareAddress();

  EXPECT_EQ(0U, port->Candidates().size());
  socket->SignalAddressReady(socket, kLocalAddr2);

  EXPECT_EQ(1U, port->Candidates().size());
}

void PortTest::TestCrossFamilyPorts(int type) {
  FakePacketSocketFactory factory;
  scoped_ptr<Port> ports[4];
  SocketAddress addresses[4] = {SocketAddress("192.168.1.3", 0),
                                SocketAddress("192.168.1.4", 0),
                                SocketAddress("2001:db8::1", 0),
                                SocketAddress("2001:db8::2", 0)};
  for (int i = 0; i < 4; i++) {
    FakeAsyncPacketSocket *socket = new FakeAsyncPacketSocket();
    if (type == SOCK_DGRAM) {
      factory.set_next_udp_socket(socket);
      ports[i].reset(CreateUdpPort(addresses[i], &factory));
    } else if (type == SOCK_STREAM) {
      factory.set_next_server_tcp_socket(socket);
      ports[i].reset(CreateTcpPort(addresses[i], &factory));
    }
    socket->set_state(AsyncPacketSocket::STATE_BINDING);
    socket->SignalAddressReady(socket, addresses[i]);
    ports[i]->PrepareAddress();
  }

  // IPv4 Port, connects to IPv6 candidate and then to IPv4 candidate.
  if (type == SOCK_STREAM) {
    FakeAsyncPacketSocket* clientsocket = new FakeAsyncPacketSocket();
    factory.set_next_client_tcp_socket(clientsocket);
  }
  Connection* c = ports[0]->CreateConnection(GetCandidate(ports[2].get()),
                                             Port::ORIGIN_MESSAGE);
  EXPECT_TRUE(NULL == c);
  EXPECT_EQ(0U, ports[0]->connections().size());
  c = ports[0]->CreateConnection(GetCandidate(ports[1].get()),
                                 Port::ORIGIN_MESSAGE);
  EXPECT_FALSE(NULL == c);
  EXPECT_EQ(1U, ports[0]->connections().size());

  // IPv6 Port, connects to IPv4 candidate and to IPv6 candidate.
  if (type == SOCK_STREAM) {
    FakeAsyncPacketSocket* clientsocket = new FakeAsyncPacketSocket();
    factory.set_next_client_tcp_socket(clientsocket);
  }
  c = ports[2]->CreateConnection(GetCandidate(ports[0].get()),
                                 Port::ORIGIN_MESSAGE);
  EXPECT_TRUE(NULL == c);
  EXPECT_EQ(0U, ports[2]->connections().size());
  c = ports[2]->CreateConnection(GetCandidate(ports[3].get()),
                                 Port::ORIGIN_MESSAGE);
  EXPECT_FALSE(NULL == c);
  EXPECT_EQ(1U, ports[2]->connections().size());
}

TEST_F(PortTest, TestSkipCrossFamilyTcp) {
  TestCrossFamilyPorts(SOCK_STREAM);
}

TEST_F(PortTest, TestSkipCrossFamilyUdp) {
  TestCrossFamilyPorts(SOCK_DGRAM);
}

// Test sending STUN messages in GICE format.
TEST_F(PortTest, TestSendStunMessageAsGice) {
  talk_base::scoped_ptr<TestPort> lport(
      CreateTestPort(kLocalAddr1, "lfrag", "lpass"));
  talk_base::scoped_ptr<TestPort> rport(
      CreateTestPort(kLocalAddr2, "rfrag", "rpass"));
  lport->SetIceProtocolType(ICEPROTO_GOOGLE);
  rport->SetIceProtocolType(ICEPROTO_GOOGLE);

  // Send a fake ping from lport to rport.
  lport->PrepareAddress();
  rport->PrepareAddress();
  ASSERT_FALSE(rport->Candidates().empty());
  Connection* conn = lport->CreateConnection(rport->Candidates()[0],
      Port::ORIGIN_MESSAGE);
  rport->CreateConnection(lport->Candidates()[0], Port::ORIGIN_MESSAGE);
  conn->Ping(0);

  // Check that it's a proper BINDING-REQUEST.
  ASSERT_TRUE_WAIT(lport->last_stun_msg() != NULL, 1000);
  IceMessage* msg = lport->last_stun_msg();
  EXPECT_EQ(STUN_BINDING_REQUEST, msg->type());
  EXPECT_FALSE(msg->IsLegacy());
  const StunByteStringAttribute* username_attr = msg->GetByteString(
      STUN_ATTR_USERNAME);
  ASSERT_TRUE(username_attr != NULL);
  EXPECT_EQ("rfraglfrag", username_attr->GetString());
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_MESSAGE_INTEGRITY) == NULL);
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_PRIORITY) == NULL);
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_FINGERPRINT) == NULL);

  // Save a copy of the BINDING-REQUEST for use below.
  talk_base::scoped_ptr<IceMessage> request(CopyStunMessage(msg));

  // Respond with a BINDING-RESPONSE.
  rport->SendBindingResponse(request.get(), lport->Candidates()[0].address());
  msg = rport->last_stun_msg();
  ASSERT_TRUE(msg != NULL);
  EXPECT_EQ(STUN_BINDING_RESPONSE, msg->type());
  EXPECT_FALSE(msg->IsLegacy());
  username_attr = msg->GetByteString(STUN_ATTR_USERNAME);
  ASSERT_TRUE(username_attr != NULL);  // GICE has a username in the response.
  EXPECT_EQ("rfraglfrag", username_attr->GetString());
  const StunAddressAttribute* addr_attr = msg->GetAddress(
      STUN_ATTR_MAPPED_ADDRESS);
  ASSERT_TRUE(addr_attr != NULL);
  EXPECT_EQ(lport->Candidates()[0].address(), addr_attr->GetAddress());
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_XOR_MAPPED_ADDRESS) == NULL);
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_MESSAGE_INTEGRITY) == NULL);
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_PRIORITY) == NULL);
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_FINGERPRINT) == NULL);

  // Respond with a BINDING-ERROR-RESPONSE. This wouldn't happen in real life,
  // but we can do it here.
  rport->SendBindingErrorResponse(request.get(),
                                  rport->Candidates()[0].address(),
                                  STUN_ERROR_SERVER_ERROR,
                                  STUN_ERROR_REASON_SERVER_ERROR);
  msg = rport->last_stun_msg();
  ASSERT_TRUE(msg != NULL);
  EXPECT_EQ(STUN_BINDING_ERROR_RESPONSE, msg->type());
  EXPECT_FALSE(msg->IsLegacy());
  username_attr = msg->GetByteString(STUN_ATTR_USERNAME);
  ASSERT_TRUE(username_attr != NULL);  // GICE has a username in the response.
  EXPECT_EQ("rfraglfrag", username_attr->GetString());
  const StunErrorCodeAttribute* error_attr = msg->GetErrorCode();
  ASSERT_TRUE(error_attr != NULL);
  // The GICE wire format for error codes is incorrect.
  EXPECT_EQ(STUN_ERROR_SERVER_ERROR_AS_GICE, error_attr->code());
  EXPECT_EQ(STUN_ERROR_SERVER_ERROR / 256, error_attr->eclass());
  EXPECT_EQ(STUN_ERROR_SERVER_ERROR % 256, error_attr->number());
  EXPECT_EQ(std::string(STUN_ERROR_REASON_SERVER_ERROR), error_attr->reason());
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_PRIORITY) == NULL);
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_MESSAGE_INTEGRITY) == NULL);
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_FINGERPRINT) == NULL);
}

// Test sending STUN messages in ICE format.
TEST_F(PortTest, TestSendStunMessageAsIce) {
  talk_base::scoped_ptr<TestPort> lport(
      CreateTestPort(kLocalAddr1, "lfrag", "lpass"));
  talk_base::scoped_ptr<TestPort> rport(
      CreateTestPort(kLocalAddr2, "rfrag", "rpass"));
  lport->SetIceProtocolType(ICEPROTO_RFC5245);
  lport->SetRole(cricket::ROLE_CONTROLLING);
  lport->SetTiebreaker(kTiebreaker1);
  rport->SetIceProtocolType(ICEPROTO_RFC5245);
  rport->SetRole(cricket::ROLE_CONTROLLED);
  rport->SetTiebreaker(kTiebreaker2);

  // Send a fake ping from lport to rport.
  lport->PrepareAddress();
  rport->PrepareAddress();
  ASSERT_FALSE(rport->Candidates().empty());
  Connection* lconn = lport->CreateConnection(
      rport->Candidates()[0], Port::ORIGIN_MESSAGE);
  Connection* rconn = rport->CreateConnection(
      lport->Candidates()[0], Port::ORIGIN_MESSAGE);
  lconn->Ping(0);

  // Check that it's a proper BINDING-REQUEST.
  ASSERT_TRUE_WAIT(lport->last_stun_msg() != NULL, 1000);
  IceMessage* msg = lport->last_stun_msg();
  EXPECT_EQ(STUN_BINDING_REQUEST, msg->type());
  EXPECT_FALSE(msg->IsLegacy());
  const StunByteStringAttribute* username_attr =
      msg->GetByteString(STUN_ATTR_USERNAME);
  ASSERT_TRUE(username_attr != NULL);
  const StunUInt32Attribute* priority_attr = msg->GetUInt32(STUN_ATTR_PRIORITY);
  ASSERT_TRUE(priority_attr != NULL);
  EXPECT_EQ(kDefaultPrflxPriority, priority_attr->value());
  EXPECT_EQ("rfrag:lfrag", username_attr->GetString());
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_MESSAGE_INTEGRITY) != NULL);
  EXPECT_TRUE(StunMessage::ValidateMessageIntegrity(
      lport->last_stun_buf()->Data(), lport->last_stun_buf()->Length(),
      "rpass"));
  const StunUInt64Attribute* ice_controlling_attr =
      msg->GetUInt64(STUN_ATTR_ICE_CONTROLLING);
  ASSERT_TRUE(ice_controlling_attr != NULL);
  EXPECT_EQ(lport->Tiebreaker(), ice_controlling_attr->value());
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_ICE_CONTROLLED) == NULL);
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_USE_CANDIDATE) != NULL);
  EXPECT_TRUE(msg->GetUInt32(STUN_ATTR_FINGERPRINT) != NULL);
  EXPECT_TRUE(StunMessage::ValidateFingerprint(
      lport->last_stun_buf()->Data(), lport->last_stun_buf()->Length()));

  // Request should not include ping count.
  ASSERT_TRUE(msg->GetUInt32(STUN_ATTR_RETRANSMIT_COUNT) == NULL);

  // Save a copy of the BINDING-REQUEST for use below.
  talk_base::scoped_ptr<IceMessage> request(CopyStunMessage(msg));

  // Respond with a BINDING-RESPONSE.
  rport->SendBindingResponse(request.get(), lport->Candidates()[0].address());
  msg = rport->last_stun_msg();
  ASSERT_TRUE(msg != NULL);
  EXPECT_EQ(STUN_BINDING_RESPONSE, msg->type());


  EXPECT_FALSE(msg->IsLegacy());
  const StunAddressAttribute* addr_attr = msg->GetAddress(
      STUN_ATTR_XOR_MAPPED_ADDRESS);
  ASSERT_TRUE(addr_attr != NULL);
  EXPECT_EQ(lport->Candidates()[0].address(), addr_attr->GetAddress());
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_MESSAGE_INTEGRITY) != NULL);
  EXPECT_TRUE(StunMessage::ValidateMessageIntegrity(
      rport->last_stun_buf()->Data(), rport->last_stun_buf()->Length(),
      "rpass"));
  EXPECT_TRUE(msg->GetUInt32(STUN_ATTR_FINGERPRINT) != NULL);
  EXPECT_TRUE(StunMessage::ValidateFingerprint(
      lport->last_stun_buf()->Data(), lport->last_stun_buf()->Length()));
  // No USERNAME or PRIORITY in ICE responses.
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_USERNAME) == NULL);
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_PRIORITY) == NULL);
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_MAPPED_ADDRESS) == NULL);
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_ICE_CONTROLLING) == NULL);
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_ICE_CONTROLLED) == NULL);
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_USE_CANDIDATE) == NULL);

  // Response should not include ping count.
  ASSERT_TRUE(msg->GetUInt32(STUN_ATTR_RETRANSMIT_COUNT) == NULL);

  // Respond with a BINDING-ERROR-RESPONSE. This wouldn't happen in real life,
  // but we can do it here.
  rport->SendBindingErrorResponse(request.get(),
                                  lport->Candidates()[0].address(),
                                  STUN_ERROR_SERVER_ERROR,
                                  STUN_ERROR_REASON_SERVER_ERROR);
  msg = rport->last_stun_msg();
  ASSERT_TRUE(msg != NULL);
  EXPECT_EQ(STUN_BINDING_ERROR_RESPONSE, msg->type());
  EXPECT_FALSE(msg->IsLegacy());
  const StunErrorCodeAttribute* error_attr = msg->GetErrorCode();
  ASSERT_TRUE(error_attr != NULL);
  EXPECT_EQ(STUN_ERROR_SERVER_ERROR, error_attr->code());
  EXPECT_EQ(std::string(STUN_ERROR_REASON_SERVER_ERROR), error_attr->reason());
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_MESSAGE_INTEGRITY) != NULL);
  EXPECT_TRUE(StunMessage::ValidateMessageIntegrity(
      rport->last_stun_buf()->Data(), rport->last_stun_buf()->Length(),
      "rpass"));
  EXPECT_TRUE(msg->GetUInt32(STUN_ATTR_FINGERPRINT) != NULL);
  EXPECT_TRUE(StunMessage::ValidateFingerprint(
      lport->last_stun_buf()->Data(), lport->last_stun_buf()->Length()));
  // No USERNAME with ICE.
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_USERNAME) == NULL);
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_PRIORITY) == NULL);

  // Testing STUN binding requests from rport --> lport, having ICE_CONTROLLED
  // and (incremented) RETRANSMIT_COUNT attributes.
  rport->Reset();
  rport->set_send_retransmit_count_attribute(true);
  rconn->Ping(0);
  rconn->Ping(0);
  rconn->Ping(0);
  ASSERT_TRUE_WAIT(rport->last_stun_msg() != NULL, 1000);
  msg = rport->last_stun_msg();
  EXPECT_EQ(STUN_BINDING_REQUEST, msg->type());
  const StunUInt64Attribute* ice_controlled_attr =
      msg->GetUInt64(STUN_ATTR_ICE_CONTROLLED);
  ASSERT_TRUE(ice_controlled_attr != NULL);
  EXPECT_EQ(rport->Tiebreaker(), ice_controlled_attr->value());
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_USE_CANDIDATE) == NULL);

  // Request should include ping count.
  const StunUInt32Attribute* retransmit_attr =
      msg->GetUInt32(STUN_ATTR_RETRANSMIT_COUNT);
  ASSERT_TRUE(retransmit_attr != NULL);
  EXPECT_EQ(2U, retransmit_attr->value());

  // Respond with a BINDING-RESPONSE.
  request.reset(CopyStunMessage(msg));
  lport->SendBindingResponse(request.get(), rport->Candidates()[0].address());
  msg = lport->last_stun_msg();

  // Response should include same ping count.
  retransmit_attr = msg->GetUInt32(STUN_ATTR_RETRANSMIT_COUNT);
  ASSERT_TRUE(retransmit_attr != NULL);
  EXPECT_EQ(2U, retransmit_attr->value());
}

TEST_F(PortTest, TestUseCandidateAttribute) {
  talk_base::scoped_ptr<TestPort> lport(
      CreateTestPort(kLocalAddr1, "lfrag", "lpass"));
  talk_base::scoped_ptr<TestPort> rport(
      CreateTestPort(kLocalAddr2, "rfrag", "rpass"));
  lport->SetIceProtocolType(ICEPROTO_RFC5245);
  lport->SetRole(cricket::ROLE_CONTROLLING);
  lport->SetTiebreaker(kTiebreaker1);
  rport->SetIceProtocolType(ICEPROTO_RFC5245);
  rport->SetRole(cricket::ROLE_CONTROLLED);
  rport->SetTiebreaker(kTiebreaker2);

  // Send a fake ping from lport to rport.
  lport->PrepareAddress();
  rport->PrepareAddress();
  ASSERT_FALSE(rport->Candidates().empty());
  Connection* lconn = lport->CreateConnection(
      rport->Candidates()[0], Port::ORIGIN_MESSAGE);
  // Set nominated flag in controlling connection.
  lconn->set_nominated(true);
  lconn->Ping(0);
  ASSERT_TRUE_WAIT(lport->last_stun_msg() != NULL, 1000);
  IceMessage* msg = lport->last_stun_msg();
  const StunUInt64Attribute* ice_controlling_attr =
      msg->GetUInt64(STUN_ATTR_ICE_CONTROLLING);
  ASSERT_TRUE(ice_controlling_attr != NULL);
  const StunByteStringAttribute* use_candidate_attr = msg->GetByteString(
      STUN_ATTR_USE_CANDIDATE);
  ASSERT_TRUE(use_candidate_attr != NULL);
}

// Test handling STUN messages in GICE format.
TEST_F(PortTest, TestHandleStunMessageAsGice) {
  // Our port will act as the "remote" port.
  talk_base::scoped_ptr<TestPort> port(
      CreateTestPort(kLocalAddr2, "rfrag", "rpass"));
  port->SetIceProtocolType(ICEPROTO_GOOGLE);

  talk_base::scoped_ptr<IceMessage> in_msg, out_msg;
  talk_base::scoped_ptr<ByteBuffer> buf(new ByteBuffer());
  talk_base::SocketAddress addr(kLocalAddr1);
  std::string username;

  // BINDING-REQUEST from local to remote with valid GICE username and no M-I.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_REQUEST,
                                             "rfraglfrag"));
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                   out_msg.accept(), &username));
  EXPECT_TRUE(out_msg.get() != NULL);  // Succeeds, since this is GICE.
  EXPECT_EQ("lfrag", username);

  // Add M-I; should be ignored and rest of message parsed normally.
  in_msg->AddMessageIntegrity("password");
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                   out_msg.accept(), &username));
  EXPECT_TRUE(out_msg.get() != NULL);
  EXPECT_EQ("lfrag", username);

  // BINDING-RESPONSE with username, as done in GICE. Should succeed.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_RESPONSE,
                                             "rfraglfrag"));
  in_msg->AddAttribute(
      new StunAddressAttribute(STUN_ATTR_MAPPED_ADDRESS, kLocalAddr2));
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                   out_msg.accept(), &username));
  EXPECT_TRUE(out_msg.get() != NULL);
  EXPECT_EQ("", username);

  // BINDING-RESPONSE without username. Should be tolerated as well.
  in_msg.reset(CreateStunMessage(STUN_BINDING_RESPONSE));
  in_msg->AddAttribute(
      new StunAddressAttribute(STUN_ATTR_MAPPED_ADDRESS, kLocalAddr2));
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                   out_msg.accept(), &username));
  EXPECT_TRUE(out_msg.get() != NULL);
  EXPECT_EQ("", username);

  // BINDING-ERROR-RESPONSE with username and error code.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_ERROR_RESPONSE,
                                             "rfraglfrag"));
  in_msg->AddAttribute(new StunErrorCodeAttribute(STUN_ATTR_ERROR_CODE,
      STUN_ERROR_SERVER_ERROR_AS_GICE, STUN_ERROR_REASON_SERVER_ERROR));
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                   out_msg.accept(), &username));
  ASSERT_TRUE(out_msg.get() != NULL);
  EXPECT_EQ("", username);
  ASSERT_TRUE(out_msg->GetErrorCode() != NULL);
  // GetStunMessage doesn't unmunge the GICE error code (happens downstream).
  EXPECT_EQ(STUN_ERROR_SERVER_ERROR_AS_GICE, out_msg->GetErrorCode()->code());
  EXPECT_EQ(std::string(STUN_ERROR_REASON_SERVER_ERROR),
      out_msg->GetErrorCode()->reason());
}

// Test handling STUN messages in ICE format.
TEST_F(PortTest, TestHandleStunMessageAsIce) {
  // Our port will act as the "remote" port.
  talk_base::scoped_ptr<TestPort> port(
      CreateTestPort(kLocalAddr2, "rfrag", "rpass"));
  port->SetIceProtocolType(ICEPROTO_RFC5245);

  talk_base::scoped_ptr<IceMessage> in_msg, out_msg;
  talk_base::scoped_ptr<ByteBuffer> buf(new ByteBuffer());
  talk_base::SocketAddress addr(kLocalAddr1);
  std::string username;

  // BINDING-REQUEST from local to remote with valid ICE username,
  // MESSAGE-INTEGRITY, and FINGERPRINT.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_REQUEST,
                                             "rfrag:lfrag"));
  in_msg->AddMessageIntegrity("rpass");
  in_msg->AddFingerprint();
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                   out_msg.accept(), &username));
  EXPECT_TRUE(out_msg.get() != NULL);
  EXPECT_EQ("lfrag", username);

  // BINDING-RESPONSE without username, with MESSAGE-INTEGRITY and FINGERPRINT.
  in_msg.reset(CreateStunMessage(STUN_BINDING_RESPONSE));
  in_msg->AddAttribute(
      new StunXorAddressAttribute(STUN_ATTR_XOR_MAPPED_ADDRESS, kLocalAddr2));
  in_msg->AddMessageIntegrity("rpass");
  in_msg->AddFingerprint();
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                   out_msg.accept(), &username));
  EXPECT_TRUE(out_msg.get() != NULL);
  EXPECT_EQ("", username);

  // BINDING-ERROR-RESPONSE without username, with error, M-I, and FINGERPRINT.
  in_msg.reset(CreateStunMessage(STUN_BINDING_ERROR_RESPONSE));
  in_msg->AddAttribute(new StunErrorCodeAttribute(STUN_ATTR_ERROR_CODE,
      STUN_ERROR_SERVER_ERROR, STUN_ERROR_REASON_SERVER_ERROR));
  in_msg->AddFingerprint();
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                   out_msg.accept(), &username));
  EXPECT_TRUE(out_msg.get() != NULL);
  EXPECT_EQ("", username);
  ASSERT_TRUE(out_msg->GetErrorCode() != NULL);
  EXPECT_EQ(STUN_ERROR_SERVER_ERROR, out_msg->GetErrorCode()->code());
  EXPECT_EQ(std::string(STUN_ERROR_REASON_SERVER_ERROR),
      out_msg->GetErrorCode()->reason());
}

// Tests handling of GICE binding requests with missing or incorrect usernames.
TEST_F(PortTest, TestHandleStunMessageAsGiceBadUsername) {
  talk_base::scoped_ptr<TestPort> port(
      CreateTestPort(kLocalAddr2, "rfrag", "rpass"));
  port->SetIceProtocolType(ICEPROTO_GOOGLE);

  talk_base::scoped_ptr<IceMessage> in_msg, out_msg;
  talk_base::scoped_ptr<ByteBuffer> buf(new ByteBuffer());
  talk_base::SocketAddress addr(kLocalAddr1);
  std::string username;

  // BINDING-REQUEST with no username.
  in_msg.reset(CreateStunMessage(STUN_BINDING_REQUEST));
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                   out_msg.accept(), &username));
  EXPECT_TRUE(out_msg.get() == NULL);
  EXPECT_EQ("", username);
  EXPECT_EQ(STUN_ERROR_BAD_REQUEST_AS_GICE, port->last_stun_error_code());

  // BINDING-REQUEST with empty username.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_REQUEST, ""));
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                   out_msg.accept(), &username));
  EXPECT_TRUE(out_msg.get() == NULL);
  EXPECT_EQ("", username);
  EXPECT_EQ(STUN_ERROR_UNAUTHORIZED_AS_GICE, port->last_stun_error_code());

  // BINDING-REQUEST with too-short username.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_REQUEST, "lfra"));
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                   out_msg.accept(), &username));
  EXPECT_TRUE(out_msg.get() == NULL);
  EXPECT_EQ("", username);
  EXPECT_EQ(STUN_ERROR_UNAUTHORIZED_AS_GICE, port->last_stun_error_code());

  // BINDING-REQUEST with reversed username.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_REQUEST,
                                             "lfragrfrag"));
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                   out_msg.accept(), &username));
  EXPECT_TRUE(out_msg.get() == NULL);
  EXPECT_EQ("", username);
  EXPECT_EQ(STUN_ERROR_UNAUTHORIZED_AS_GICE, port->last_stun_error_code());

  // BINDING-REQUEST with garbage username.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_REQUEST,
                                             "abcdefgh"));
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                   out_msg.accept(), &username));
  EXPECT_TRUE(out_msg.get() == NULL);
  EXPECT_EQ("", username);
  EXPECT_EQ(STUN_ERROR_UNAUTHORIZED_AS_GICE, port->last_stun_error_code());
}

// Tests handling of ICE binding requests with missing or incorrect usernames.
TEST_F(PortTest, TestHandleStunMessageAsIceBadUsername) {
  talk_base::scoped_ptr<TestPort> port(
      CreateTestPort(kLocalAddr2, "rfrag", "rpass"));
  port->SetIceProtocolType(ICEPROTO_RFC5245);

  talk_base::scoped_ptr<IceMessage> in_msg, out_msg;
  talk_base::scoped_ptr<ByteBuffer> buf(new ByteBuffer());
  talk_base::SocketAddress addr(kLocalAddr1);
  std::string username;

  // BINDING-REQUEST with no username.
  in_msg.reset(CreateStunMessage(STUN_BINDING_REQUEST));
  in_msg->AddMessageIntegrity("rpass");
  in_msg->AddFingerprint();
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                   out_msg.accept(), &username));
  EXPECT_TRUE(out_msg.get() == NULL);
  EXPECT_EQ("", username);
  EXPECT_EQ(STUN_ERROR_BAD_REQUEST, port->last_stun_error_code());

  // BINDING-REQUEST with empty username.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_REQUEST, ""));
  in_msg->AddMessageIntegrity("rpass");
  in_msg->AddFingerprint();
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                   out_msg.accept(), &username));
  EXPECT_TRUE(out_msg.get() == NULL);
  EXPECT_EQ("", username);
  EXPECT_EQ(STUN_ERROR_UNAUTHORIZED, port->last_stun_error_code());

  // BINDING-REQUEST with too-short username.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_REQUEST, "rfra"));
  in_msg->AddMessageIntegrity("rpass");
  in_msg->AddFingerprint();
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                   out_msg.accept(), &username));
  EXPECT_TRUE(out_msg.get() == NULL);
  EXPECT_EQ("", username);
  EXPECT_EQ(STUN_ERROR_UNAUTHORIZED, port->last_stun_error_code());

  // BINDING-REQUEST with reversed username.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_REQUEST,
                                            "lfrag:rfrag"));
  in_msg->AddMessageIntegrity("rpass");
  in_msg->AddFingerprint();
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                   out_msg.accept(), &username));
  EXPECT_TRUE(out_msg.get() == NULL);
  EXPECT_EQ("", username);
  EXPECT_EQ(STUN_ERROR_UNAUTHORIZED, port->last_stun_error_code());

  // BINDING-REQUEST with garbage username.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_REQUEST,
                                             "abcd:efgh"));
  in_msg->AddMessageIntegrity("rpass");
  in_msg->AddFingerprint();
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                   out_msg.accept(), &username));
  EXPECT_TRUE(out_msg.get() == NULL);
  EXPECT_EQ("", username);
  EXPECT_EQ(STUN_ERROR_UNAUTHORIZED, port->last_stun_error_code());
}

// Test handling STUN messages (as ICE) with missing or malformed M-I.
TEST_F(PortTest, TestHandleStunMessageAsIceBadMessageIntegrity) {
  // Our port will act as the "remote" port.
  talk_base::scoped_ptr<TestPort> port(
      CreateTestPort(kLocalAddr2, "rfrag", "rpass"));
  port->SetIceProtocolType(ICEPROTO_RFC5245);

  talk_base::scoped_ptr<IceMessage> in_msg, out_msg;
  talk_base::scoped_ptr<ByteBuffer> buf(new ByteBuffer());
  talk_base::SocketAddress addr(kLocalAddr1);
  std::string username;

  // BINDING-REQUEST from local to remote with valid ICE username and
  // FINGERPRINT, but no MESSAGE-INTEGRITY.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_REQUEST,
                                             "rfrag:lfrag"));
  in_msg->AddFingerprint();
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                   out_msg.accept(), &username));
  EXPECT_TRUE(out_msg.get() == NULL);
  EXPECT_EQ("", username);
  EXPECT_EQ(STUN_ERROR_BAD_REQUEST, port->last_stun_error_code());

  // BINDING-REQUEST from local to remote with valid ICE username and
  // FINGERPRINT, but invalid MESSAGE-INTEGRITY.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_REQUEST,
                                             "rfrag:lfrag"));
  in_msg->AddMessageIntegrity("invalid");
  in_msg->AddFingerprint();
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                   out_msg.accept(), &username));
  EXPECT_TRUE(out_msg.get() == NULL);
  EXPECT_EQ("", username);
  EXPECT_EQ(STUN_ERROR_UNAUTHORIZED, port->last_stun_error_code());

  // TODO: BINDING-RESPONSES and BINDING-ERROR-RESPONSES are checked
  // by the Connection, not the Port, since they require the remote username.
  // Change this test to pass in data via Connection::OnReadPacket instead.
}

// Test handling STUN messages (as ICE) with missing or malformed FINGERPRINT.
TEST_F(PortTest, TestHandleStunMessageAsIceBadFingerprint) {
  // Our port will act as the "remote" port.
  talk_base::scoped_ptr<TestPort> port(
      CreateTestPort(kLocalAddr2, "rfrag", "rpass"));
  port->SetIceProtocolType(ICEPROTO_RFC5245);

  talk_base::scoped_ptr<IceMessage> in_msg, out_msg;
  talk_base::scoped_ptr<ByteBuffer> buf(new ByteBuffer());
  talk_base::SocketAddress addr(kLocalAddr1);
  std::string username;

  // BINDING-REQUEST from local to remote with valid ICE username and
  // MESSAGE-INTEGRITY, but no FINGERPRINT; GetStunMessage should fail.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_REQUEST,
                                             "rfrag:lfrag"));
  in_msg->AddMessageIntegrity("rpass");
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_FALSE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                    out_msg.accept(), &username));
  EXPECT_EQ(0, port->last_stun_error_code());

  // Now, add a fingerprint, but munge the message so it's not valid.
  in_msg->AddFingerprint();
  in_msg->SetTransactionID("TESTTESTBADD");
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_FALSE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                    out_msg.accept(), &username));
  EXPECT_EQ(0, port->last_stun_error_code());

  // Valid BINDING-RESPONSE, except no FINGERPRINT.
  in_msg.reset(CreateStunMessage(STUN_BINDING_RESPONSE));
  in_msg->AddAttribute(
      new StunXorAddressAttribute(STUN_ATTR_XOR_MAPPED_ADDRESS, kLocalAddr2));
  in_msg->AddMessageIntegrity("rpass");
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_FALSE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                    out_msg.accept(), &username));
  EXPECT_EQ(0, port->last_stun_error_code());

  // Now, add a fingerprint, but munge the message so it's not valid.
  in_msg->AddFingerprint();
  in_msg->SetTransactionID("TESTTESTBADD");
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_FALSE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                    out_msg.accept(), &username));
  EXPECT_EQ(0, port->last_stun_error_code());

  // Valid BINDING-ERROR-RESPONSE, except no FINGERPRINT.
  in_msg.reset(CreateStunMessage(STUN_BINDING_ERROR_RESPONSE));
  in_msg->AddAttribute(new StunErrorCodeAttribute(STUN_ATTR_ERROR_CODE,
      STUN_ERROR_SERVER_ERROR, STUN_ERROR_REASON_SERVER_ERROR));
  in_msg->AddMessageIntegrity("rpass");
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_FALSE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                    out_msg.accept(), &username));
  EXPECT_EQ(0, port->last_stun_error_code());

  // Now, add a fingerprint, but munge the message so it's not valid.
  in_msg->AddFingerprint();
  in_msg->SetTransactionID("TESTTESTBADD");
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_FALSE(port->GetStunMessage(buf->Data(), buf->Length(), addr,
                                    out_msg.accept(), &username));
  EXPECT_EQ(0, port->last_stun_error_code());
}

// Test handling of STUN binding indication messages (as ICE). STUN binding
// indications are allowed only to the connection which is in read mode.
TEST_F(PortTest, TestHandleStunBindingIndication) {
  talk_base::scoped_ptr<TestPort> lport(
      CreateTestPort(kLocalAddr2, "lfrag", "lpass"));
  lport->SetIceProtocolType(ICEPROTO_RFC5245);
  lport->SetRole(cricket::ROLE_CONTROLLING);
  lport->SetTiebreaker(kTiebreaker1);

  // Verifying encoding and decoding STUN indication message.
  talk_base::scoped_ptr<IceMessage> in_msg, out_msg;
  talk_base::scoped_ptr<ByteBuffer> buf(new ByteBuffer());
  talk_base::SocketAddress addr(kLocalAddr1);
  std::string username;

  in_msg.reset(CreateStunMessage(STUN_BINDING_INDICATION));
  in_msg->AddFingerprint();
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(lport->GetStunMessage(buf->Data(), buf->Length(), addr,
                                    out_msg.accept(), &username));
  EXPECT_TRUE(out_msg.get() != NULL);
  EXPECT_EQ(out_msg->type(), STUN_BINDING_INDICATION);
  EXPECT_EQ("", username);

  // Verify connection can handle STUN indication and updates
  // last_ping_received.
  talk_base::scoped_ptr<TestPort> rport(
      CreateTestPort(kLocalAddr2, "rfrag", "rpass"));
  rport->SetIceProtocolType(ICEPROTO_RFC5245);
  rport->SetRole(cricket::ROLE_CONTROLLED);
  rport->SetTiebreaker(kTiebreaker2);

  lport->PrepareAddress();
  rport->PrepareAddress();
  ASSERT_FALSE(lport->Candidates().empty());
  ASSERT_FALSE(rport->Candidates().empty());

  Connection* lconn = lport->CreateConnection(rport->Candidates()[0],
                                              Port::ORIGIN_MESSAGE);
  Connection* rconn = rport->CreateConnection(lport->Candidates()[0],
                                              Port::ORIGIN_MESSAGE);
  rconn->Ping(0);

  ASSERT_TRUE_WAIT(rport->last_stun_msg() != NULL, 1000);
  IceMessage* msg = rport->last_stun_msg();
  EXPECT_EQ(STUN_BINDING_REQUEST, msg->type());
  // Send rport binding request to lport.
  lconn->OnReadPacket(rport->last_stun_buf()->Data(),
                      rport->last_stun_buf()->Length());
  ASSERT_TRUE_WAIT(lport->last_stun_msg() != NULL, 1000);
  EXPECT_EQ(STUN_BINDING_RESPONSE, lport->last_stun_msg()->type());
  uint32 last_ping_received1 = lconn->last_ping_received();

  // Adding a delay of 100ms.
  talk_base::Thread::Current()->ProcessMessages(100);
  // Pinging lconn using stun indication message.
  lconn->OnReadPacket(buf->Data(), buf->Length());
  uint32 last_ping_received2 = lconn->last_ping_received();
  EXPECT_GT(last_ping_received2, last_ping_received1);
}

TEST_F(PortTest, TestComputeCandidatePriority) {
  talk_base::scoped_ptr<TestPort> port(
      CreateTestPort(kLocalAddr1, "name", "pass"));
  port->set_type_preference(90);
  port->set_component(177);
  port->AddCandidateAddress(SocketAddress("192.168.1.4", 1234));
  port->AddCandidateAddress(SocketAddress("2001:db8::1234", 1234));
  port->AddCandidateAddress(SocketAddress("fc12:3456::1234", 1234));
  port->AddCandidateAddress(SocketAddress("::ffff:192.168.1.4", 1234));
  port->AddCandidateAddress(SocketAddress("::192.168.1.4", 1234));
  port->AddCandidateAddress(SocketAddress("2002::1234:5678", 1234));
  port->AddCandidateAddress(SocketAddress("2001::1234:5678", 1234));
  port->AddCandidateAddress(SocketAddress("fecf::1234:5678", 1234));
  port->AddCandidateAddress(SocketAddress("3ffe::1234:5678", 1234));
  // These should all be:
  // (90 << 24) | ([rfc3484 pref value] << 8) | (256 - 177)
  uint32 expected_priority_v4 = 1509957199U;
  uint32 expected_priority_v6 = 1509959759U;
  uint32 expected_priority_ula = 1509962319U;
  uint32 expected_priority_v4mapped = expected_priority_v4;
  uint32 expected_priority_v4compat = 1509949775U;
  uint32 expected_priority_6to4 = 1509954639U;
  uint32 expected_priority_teredo = 1509952079U;
  uint32 expected_priority_sitelocal = 1509949775U;
  uint32 expected_priority_6bone = 1509949775U;
  ASSERT_EQ(expected_priority_v4, port->Candidates()[0].priority());
  ASSERT_EQ(expected_priority_v6, port->Candidates()[1].priority());
  ASSERT_EQ(expected_priority_ula, port->Candidates()[2].priority());
  ASSERT_EQ(expected_priority_v4mapped, port->Candidates()[3].priority());
  ASSERT_EQ(expected_priority_v4compat, port->Candidates()[4].priority());
  ASSERT_EQ(expected_priority_6to4, port->Candidates()[5].priority());
  ASSERT_EQ(expected_priority_teredo, port->Candidates()[6].priority());
  ASSERT_EQ(expected_priority_sitelocal, port->Candidates()[7].priority());
  ASSERT_EQ(expected_priority_6bone, port->Candidates()[8].priority());
}

TEST_F(PortTest, TestPortProxyProperties) {
  talk_base::scoped_ptr<TestPort> port(
      CreateTestPort(kLocalAddr1, "name", "pass"));
  port->SetRole(cricket::ROLE_CONTROLLING);
  port->SetTiebreaker(kTiebreaker1);

  // Create a proxy port.
  talk_base::scoped_ptr<PortProxy> proxy(new PortProxy());
  proxy->set_impl(port.get());
  EXPECT_EQ(port->Type(), proxy->Type());
  EXPECT_EQ(port->Network(), proxy->Network());
  EXPECT_EQ(port->Role(), proxy->Role());
  EXPECT_EQ(port->Tiebreaker(), proxy->Tiebreaker());
}

// In the case of shared socket, one port may be shared by local and stun.
// Test that candidates with different types will have different foundation.
TEST_F(PortTest, TestFoundation) {
  talk_base::scoped_ptr<TestPort> testport(
      CreateTestPort(kLocalAddr1, "name", "pass"));
  testport->AddCandidateAddress(kLocalAddr1, kLocalAddr1,
                                LOCAL_PORT_TYPE, false);
  testport->AddCandidateAddress(kLocalAddr2, kLocalAddr1,
                                STUN_PORT_TYPE, true);
  EXPECT_NE(testport->Candidates()[0].foundation(),
            testport->Candidates()[1].foundation());
}

// TODO(mallinath) - Enable below test and add related address
// tests for STUN and TURN ports.
TEST_F(PortTest, DISABLED_TestRelatedAddressAndFoundation) {
  talk_base::scoped_ptr<UDPPort> udpport(CreateUdpPort(kLocalAddr1));
  udpport->PrepareAddress();
  // For UDPPort, related address will be empty.
  EXPECT_TRUE(udpport->Candidates()[0].related_address().IsNil());
  talk_base::scoped_ptr<UDPPort> udpport1(CreateUdpPort(kLocalAddr1));
  udpport1->PrepareAddress();
  // Compare foundation of candidates from both ports.
  // TODO: Update this check with a STUN port which has the same
  // base of UDP Port. This will happen once we have a common socket for all
  // ports.
  EXPECT_EQ(udpport->Candidates()[0].foundation(),
            udpport1->Candidates()[0].foundation());
  talk_base::scoped_ptr<TestPort> testport(
      CreateTestPort(kLocalAddr1, "name", "pass"));
  // Test port is behaving like a stun port, where candidate address is
  // will have a different related address.
  testport->set_related_address(kLocalAddr2);
  testport->PrepareAddress();
  // Foundation of udpport and testport must be different as their types are
  // different, even though the same base address kLocalAddr1.
  EXPECT_NE(udpport->Candidates()[0].foundation(),
            testport->Candidates()[0].foundation());
  EXPECT_EQ_WAIT(testport->Candidates()[0].related_address().ipaddr(),
                 kLocalAddr2.ipaddr(), kTimeout);
  talk_base::scoped_ptr<RelayPort> relayport(CreateGturnPort(kLocalAddr2));
  relayport->AddExternalAddress(ProtocolAddress(kRelayUdpIntAddr, PROTO_UDP));
  relayport->AddExternalAddress(ProtocolAddress(kRelayTcpIntAddr, PROTO_TCP));
  relayport->AddExternalAddress(
      ProtocolAddress(kRelaySslTcpIntAddr, PROTO_SSLTCP));
  relayport->AddExternalAddress(ProtocolAddress(kLocalAddr1, PROTO_UDP));
  relayport->set_related_address(kLocalAddr1);
  EXPECT_EQ_WAIT(kLocalAddr1.ipaddr(),
                 relayport->Candidates()[0].related_address().ipaddr(),
                 kTimeout);
  EXPECT_EQ_WAIT(kLocalAddr1.ipaddr(),
                 relayport->Candidates()[1].related_address().ipaddr(),
                 kTimeout);
  EXPECT_EQ_WAIT(kLocalAddr1.ipaddr(),
                 relayport->Candidates()[2].related_address().ipaddr(),
                 kTimeout);
  EXPECT_EQ_WAIT(kLocalAddr1.ipaddr(),
                 relayport->Candidates()[3].related_address().ipaddr(),
                 kTimeout);
  // Relay candidate base will be the candidate itself. Hence all candidates
  // belonging to relay candidates will have different foundation.
  EXPECT_NE(relayport->Candidates()[0].foundation(),
            relayport->Candidates()[1].foundation());
  EXPECT_NE(relayport->Candidates()[2].foundation(),
            relayport->Candidates()[3].foundation());
}

// Test priority value overflow handling when preference is set to 3.
TEST_F(PortTest, TestCandidatePreference) {
  cricket::Candidate cand1;
  cand1.set_preference(3);
  cricket::Candidate cand2;
  cand2.set_preference(1);
  EXPECT_TRUE(cand1.preference() > cand2.preference());
}

// Test the Connection priority is calculated correctly.
TEST_F(PortTest, TestConnectionPriority) {
  talk_base::scoped_ptr<TestPort> lport(
      CreateTestPort(kLocalAddr1, "lfrag", "lpass"));
  lport->set_type_preference(cricket::ICE_TYPE_PREFERENCE_HOST);
  talk_base::scoped_ptr<TestPort> rport(
      CreateTestPort(kLocalAddr2, "rfrag", "rpass"));
  rport->set_type_preference(cricket::ICE_TYPE_PREFERENCE_RELAY);
  lport->set_component(123);
  lport->AddCandidateAddress(SocketAddress("192.168.1.4", 1234));
  rport->set_component(23);
  rport->AddCandidateAddress(SocketAddress("10.1.1.100", 1234));

  EXPECT_EQ(0x7E001E85U, lport->Candidates()[0].priority());
  EXPECT_EQ(0x1EE9U, rport->Candidates()[0].priority());

  // RFC 5245
  // pair priority = 2^32*MIN(G,D) + 2*MAX(G,D) + (G>D?1:0)
  lport->SetRole(cricket::ROLE_CONTROLLING);
  rport->SetRole(cricket::ROLE_CONTROLLED);
  Connection* lconn = lport->CreateConnection(
      rport->Candidates()[0], Port::ORIGIN_MESSAGE);
#if defined(WIN32)
  EXPECT_EQ(0x1EE9FC003D0BU, lconn->priority());
#else
  EXPECT_EQ(0x1EE9FC003D0BLLU, lconn->priority());
#endif

  lport->SetRole(cricket::ROLE_CONTROLLED);
  rport->SetRole(cricket::ROLE_CONTROLLING);
  Connection* rconn = rport->CreateConnection(
      lport->Candidates()[0], Port::ORIGIN_MESSAGE);
#if defined(WIN32)
  EXPECT_EQ(0x1EE9FC003D0AU, rconn->priority());
#else
  EXPECT_EQ(0x1EE9FC003D0ALLU, rconn->priority());
#endif
}

TEST_F(PortTest, TestWritableState) {
  UDPPort* port1 = CreateUdpPort(kLocalAddr1);
  UDPPort* port2 = CreateUdpPort(kLocalAddr2);

  // Set up channels.
  TestChannel ch1(port1, port2);
  TestChannel ch2(port2, port1);

  // Acquire addresses.
  ch1.Start();
  ch2.Start();
  ASSERT_EQ_WAIT(1, ch1.address_count(), kTimeout);
  ASSERT_EQ_WAIT(1, ch2.address_count(), kTimeout);

  // Send a ping from src to dst.
  ch1.CreateConnection();
  ASSERT_TRUE(ch1.conn() != NULL);
  EXPECT_EQ(Connection::STATE_WRITE_INIT, ch1.conn()->write_state());
  EXPECT_TRUE_WAIT(ch1.conn()->connected(), kTimeout);  // for TCP connect
  ch1.Ping();
  WAIT(!ch2.remote_address().IsNil(), kTimeout);

  // Data should be unsendable until the connection is accepted.
  char data[] = "abcd";
  int data_size = ARRAY_SIZE(data);
  EXPECT_EQ(SOCKET_ERROR, ch1.conn()->Send(data, data_size));

  // Accept the connection to return the binding response, transition to
  // writable, and allow data to be sent.
  ch2.AcceptConnection();
  EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, ch1.conn()->write_state(),
                 kTimeout);
  EXPECT_EQ(data_size, ch1.conn()->Send(data, data_size));

  // Ask the connection to update state as if enough time has passed to lose
  // full writability and 5 pings went unresponded to. We'll accomplish the
  // latter by sending pings but not pumping messages.
  for (uint32 i = 1; i <= CONNECTION_WRITE_CONNECT_FAILURES; ++i) {
    ch1.Ping(i);
  }
  uint32 unreliable_timeout_delay = CONNECTION_WRITE_CONNECT_TIMEOUT + 500u;
  ch1.conn()->UpdateState(unreliable_timeout_delay);
  EXPECT_EQ(Connection::STATE_WRITE_UNRELIABLE, ch1.conn()->write_state());

  // Data should be able to be sent in this state.
  EXPECT_EQ(data_size, ch1.conn()->Send(data, data_size));

  // And now allow the other side to process the pings and send binding
  // responses.
  EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, ch1.conn()->write_state(),
                 kTimeout);

  // Wait long enough for a full timeout (past however long we've already
  // waited).
  for (uint32 i = 1; i <= CONNECTION_WRITE_CONNECT_FAILURES; ++i) {
    ch1.Ping(unreliable_timeout_delay + i);
  }
  ch1.conn()->UpdateState(unreliable_timeout_delay + CONNECTION_WRITE_TIMEOUT +
                          500u);
  EXPECT_EQ(Connection::STATE_WRITE_TIMEOUT, ch1.conn()->write_state());

  // Now that the connection has completely timed out, data send should fail.
  EXPECT_EQ(SOCKET_ERROR, ch1.conn()->Send(data, data_size));

  ch1.Stop();
  ch2.Stop();
}

TEST_F(PortTest, TestTimeoutForNeverWritable) {
  UDPPort* port1 = CreateUdpPort(kLocalAddr1);
  UDPPort* port2 = CreateUdpPort(kLocalAddr2);

  // Set up channels.
  TestChannel ch1(port1, port2);
  TestChannel ch2(port2, port1);

  // Acquire addresses.
  ch1.Start();
  ch2.Start();

  ch1.CreateConnection();
  ASSERT_TRUE(ch1.conn() != NULL);
  EXPECT_EQ(Connection::STATE_WRITE_INIT, ch1.conn()->write_state());

  // Attempt to go directly to write timeout.
  for (uint32 i = 1; i <= CONNECTION_WRITE_CONNECT_FAILURES; ++i) {
    ch1.Ping(i);
  }
  ch1.conn()->UpdateState(CONNECTION_WRITE_TIMEOUT + 500u);
  EXPECT_EQ(Connection::STATE_WRITE_TIMEOUT, ch1.conn()->write_state());
}

// TODO(ronghuawu): Add unit tests to verify the RelayEntry::OnConnect.
