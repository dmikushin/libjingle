/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
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

#ifndef TALK_P2P_BASE_STUN_H_
#define TALK_P2P_BASE_STUN_H_

// This file contains classes for dealing with the STUN protocol, as specified
// in RFC 5389, and its descendants.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bytebuffer.h"
#include "base/socketaddress.h"

namespace cricket {

// These are the types of STUN messages defined in RFC 5389.
enum StunMessageType {
  STUN_BINDING_REQUEST                  = 0x0001,
  STUN_BINDING_INDICATION               = 0x0010,
  STUN_BINDING_RESPONSE                 = 0x0101,
  STUN_BINDING_ERROR_RESPONSE           = 0x0111,
};

// These are all known STUN attributes, defined in RFC 5389 and elsewhere.
// Next to each is the name of the class (T is StunTAttribute) that implements
// that type.
// RETRANSMIT_COUNT is the number of outstanding pings without a response at
// the time the packet is generated.
enum StunAttributeType {
  STUN_ATTR_MAPPED_ADDRESS              = 0x0001,  // Address
  STUN_ATTR_USERNAME                    = 0x0006,  // ByteString
  STUN_ATTR_MESSAGE_INTEGRITY           = 0x0008,  // ByteString, 20 bytes
  STUN_ATTR_ERROR_CODE                  = 0x0009,  // ErrorCode
  STUN_ATTR_UNKNOWN_ATTRIBUTES          = 0x000a,  // UInt16List
  STUN_ATTR_REALM                       = 0x0014,  // ByteString
  STUN_ATTR_NONCE                       = 0x0015,  // ByteString
  STUN_ATTR_XOR_MAPPED_ADDRESS          = 0x0020,  // XorAddress
  STUN_ATTR_SOFTWARE                    = 0x8022,  // ByteString
  STUN_ATTR_ALTERNATE_SERVER            = 0x8023,  // ByteString
  STUN_ATTR_FINGERPRINT                 = 0x8028,  // UInt32
  STUN_ATTR_RETRANSMIT_COUNT            = 0xFF00   // UInt32
};

// These are the types of the values associated with the attributes above.
// This allows us to perform some basic validation when reading or adding
// attributes. Note that these values are for our own use, and not defined in
// RFC 5389.
enum StunAttributeValueType {
  STUN_VALUE_UNKNOWN                    = 0,
  STUN_VALUE_ADDRESS                    = 1,
  STUN_VALUE_XOR_ADDRESS                = 2,
  STUN_VALUE_UINT32                     = 3,
  STUN_VALUE_UINT64                     = 4,
  STUN_VALUE_BYTE_STRING                = 5,
  STUN_VALUE_ERROR_CODE                 = 6,
  STUN_VALUE_UINT16_LIST                = 7
};

// These are the types of STUN addresses defined in RFC 5389.
enum StunAddressFamily {
  // NB: UNDEF is not part of the STUN spec.
  STUN_ADDRESS_UNDEF                    = 0,
  STUN_ADDRESS_IPV4                     = 1,
  STUN_ADDRESS_IPV6                     = 2
};

// These are the types of STUN error codes defined in RFC 5389.
enum StunErrorCode {
  STUN_ERROR_TRY_ALTERNATE              = 300,
  STUN_ERROR_BAD_REQUEST                = 400,
  STUN_ERROR_UNAUTHORIZED               = 401,
  STUN_ERROR_UNKNOWN_ATTRIBUTE          = 420,
  STUN_ERROR_STALE_CREDENTIALS          = 430,  // GICE only
  STUN_ERROR_STALE_NONCE                = 438,
  STUN_ERROR_SERVER_ERROR               = 500,
  STUN_ERROR_GLOBAL_FAILURE             = 600
};

// Strings for the error codes above.
extern const char STUN_ERROR_REASON_BAD_REQUEST[];
extern const char STUN_ERROR_REASON_UNAUTHORIZED[];
extern const char STUN_ERROR_REASON_UNKNOWN_ATTRIBUTE[];
extern const char STUN_ERROR_REASON_STALE_CREDENTIALS[];
extern const char STUN_ERROR_REASON_STALE_NONCE[];
extern const char STUN_ERROR_REASON_SERVER_ERROR[];

// The mask used to determine whether a STUN message is a request/response etc.
const uint32 kStunTypeMask = 0x0110;

// STUN Attribute header length.
const size_t kStunAttributeHeaderSize = 4;

// Following values correspond to RFC5389.
const size_t kStunHeaderSize = 20;
const size_t kStunTransactionIdOffset = 8;
const size_t kStunTransactionIdLength = 12;
const uint32 kStunMagicCookie = 0x2112A442;
const size_t kStunMagicCookieLength = sizeof(kStunMagicCookie);

// Following value corresponds to an earlier version of STUN from
// RFC3489.
const size_t kStunLegacyTransactionIdLength = 16;

// STUN Message Integrity HMAC length.
const size_t kStunMessageIntegritySize = 20;

class StunAttribute;
class StunAddressAttribute;
class StunXorAddressAttribute;
class StunUInt32Attribute;
class StunUInt64Attribute;
class StunByteStringAttribute;
class StunErrorCodeAttribute;
class StunUInt16ListAttribute;

// Records a complete STUN/TURN message.  Each message consists of a type and
// any number of attributes.  Each attribute is parsed into an instance of an
// appropriate class (see above).  The Get* methods will return instances of
// that attribute class.
class StunMessage {
 public:
  StunMessage();
  virtual ~StunMessage();

  int type() const { return type_; }
  size_t length() const { return length_; }
  const std::string& transaction_id() const { return transaction_id_; }

  // Returns true if the message confirms to RFC3489 rather than
  // RFC5389. The main difference between two version of the STUN
  // protocol is the presence of the magic cookie and different length
  // of transaction ID. For outgoing packets version of the protocol
  // is determined by the lengths of the transaction ID.
  bool IsLegacy() const;

  void SetType(int type) { type_ = static_cast<uint16>(type); }
  bool SetTransactionID(const std::string& str);

  // Gets the desired attribute value, or NULL if no such attribute type exists.
  const StunAddressAttribute* GetAddress(int type) const;
  const StunUInt32Attribute* GetUInt32(int type) const;
  const StunUInt64Attribute* GetUInt64(int type) const;
  const StunByteStringAttribute* GetByteString(int type) const;

  // Gets these specific attribute values.
  const StunErrorCodeAttribute* GetErrorCode() const;
  const StunUInt16ListAttribute* GetUnknownAttributes() const;

  // Takes ownership of the specified attribute, verifies it is of the correct
  // type, and adds it to the message. The return value indicates whether this
  // was successful.
  bool AddAttribute(StunAttribute* attr);

  // Validates that a raw STUN message has a correct MESSAGE-INTEGRITY value.
  // This can't currently be done on a StunMessage, since it is affected by
  // padding data (which we discard when reading a StunMessage).
  static bool ValidateMessageIntegrity(const char* data, size_t size,
                                       const std::string& password);
  // Adds a MESSAGE-INTEGRITY attribute that is valid for the current message.
  bool AddMessageIntegrity(const std::string& password);
  bool AddMessageIntegrity(const char* key, size_t keylen);

  // Verifies that a given buffer is STUN by checking for a correct FINGERPRINT.
  static bool ValidateFingerprint(const char* data, size_t size);

  // Adds a FINGERPRINT attribute that is valid for the current message.
  bool AddFingerprint();

  // Parses the STUN packet in the given buffer and records it here. The
  // return value indicates whether this was successful.
  bool Read(talk_base::ByteBuffer* buf);

  // Writes this object into a STUN packet. The return value indicates whether
  // this was successful.
  bool Write(talk_base::ByteBuffer* buf) const;

  // Creates an empty message. Overridable by derived classes.
  virtual StunMessage* CreateNew() const { return new StunMessage(); }

 protected:
  // Verifies that the given attribute is allowed for this message.
  virtual StunAttributeValueType GetAttributeValueType(int type) const;

 private:
  StunAttribute* CreateAttribute(int type, size_t length) /* const*/;
  const StunAttribute* GetAttribute(int type) const;
  static bool IsValidTransactionId(const std::string& transaction_id);

  uint16 type_;
  uint16 length_;
  std::string transaction_id_;
  std::vector<StunAttribute*>* attrs_;
};

// Base class for all STUN/TURN attributes.
class StunAttribute {
 public:
  virtual ~StunAttribute() {
  }

  int type() const { return type_; }
  size_t length() const { return length_; }

  // Return the type of this attribute.
  virtual StunAttributeValueType value_type() const = 0;

  // Only XorAddressAttribute needs this so far.
  virtual void SetOwner(StunMessage* owner) {}

  // Reads the body (not the type or length) for this type of attribute from
  // the given buffer.  Return value is true if successful.
  virtual bool Read(talk_base::ByteBuffer* buf) = 0;

  // Writes the body (not the type or length) to the given buffer.  Return
  // value is true if successful.
  virtual bool Write(talk_base::ByteBuffer* buf) const = 0;

  // Creates an attribute object with the given type and smallest length.
  static StunAttribute* Create(StunAttributeValueType value_type, uint16 type,
                               uint16 length, StunMessage* owner);
  // TODO: Allow these create functions to take parameters, to reduce
  // the amount of work callers need to do to initialize attributes.
  static StunAddressAttribute* CreateAddress(uint16 type);
  static StunXorAddressAttribute* CreateXorAddress(uint16 type);
  static StunUInt32Attribute* CreateUInt32(uint16 type);
  static StunUInt64Attribute* CreateUInt64(uint16 type);
  static StunByteStringAttribute* CreateByteString(uint16 type);
  static StunErrorCodeAttribute* CreateErrorCode();
  static StunUInt16ListAttribute* CreateUnknownAttributes();

 protected:
  StunAttribute(uint16 type, uint16 length);
  void SetLength(uint16 length) { length_ = length; }
  void WritePadding(talk_base::ByteBuffer* buf) const;
  void ConsumePadding(talk_base::ByteBuffer* buf) const;

 private:
  uint16 type_;
  uint16 length_;
};

// Implements STUN attributes that record an Internet address.
class StunAddressAttribute : public StunAttribute {
 public:
  static const uint16 SIZE_UNDEF = 0;
  static const uint16 SIZE_IP4 = 8;
  static const uint16 SIZE_IP6 = 20;
  StunAddressAttribute(uint16 type, const talk_base::SocketAddress& addr);
  StunAddressAttribute(uint16 type, uint16 length);

  virtual StunAttributeValueType value_type() const {
    return STUN_VALUE_ADDRESS;
  }

  StunAddressFamily family() const {
    switch (address_.ipaddr().family()) {
      case AF_INET:
        return STUN_ADDRESS_IPV4;
      case AF_INET6:
        return STUN_ADDRESS_IPV6;
    }
    return STUN_ADDRESS_UNDEF;
  }

  const talk_base::SocketAddress& GetAddress() const { return address_; }
  const talk_base::IPAddress& ipaddr() const { return address_.ipaddr(); }
  uint16 port() const { return address_.port(); }

  void SetAddress(const talk_base::SocketAddress& addr) {
    address_ = addr;
    EnsureAddressLength();
  }
  void SetIP(const talk_base::IPAddress& ip) {
    address_.SetIP(ip);
    EnsureAddressLength();
  }
  void SetPort(uint16 port) { address_.SetPort(port); }

  virtual bool Read(talk_base::ByteBuffer* buf);
  virtual bool Write(talk_base::ByteBuffer* buf) const;

 private:
  void EnsureAddressLength() {
    switch (family()) {
      case STUN_ADDRESS_IPV4: {
        SetLength(SIZE_IP4);
        break;
      }
      case STUN_ADDRESS_IPV6: {
        SetLength(SIZE_IP6);
        break;
      }
      default: {
        SetLength(SIZE_UNDEF);
        break;
      }
    }
  }
  talk_base::SocketAddress address_;
};

// Implements STUN attributes that record an Internet address. When encoded
// in a STUN message, the address contained in this attribute is XORed with the
// transaction ID of the message.
class StunXorAddressAttribute : public StunAddressAttribute {
 public:
  StunXorAddressAttribute(uint16 type, const talk_base::SocketAddress& addr);
  StunXorAddressAttribute(uint16 type, uint16 length,
                          StunMessage* owner);

  virtual StunAttributeValueType value_type() const {
    return STUN_VALUE_XOR_ADDRESS;
  }
  virtual void SetOwner(StunMessage* owner) {
    owner_ = owner;
  }
  virtual bool Read(talk_base::ByteBuffer* buf);
  virtual bool Write(talk_base::ByteBuffer* buf) const;

 private:
  talk_base::IPAddress GetXoredIP() const;
  StunMessage* owner_;
};

// Implements STUN attributes that record a 32-bit integer.
class StunUInt32Attribute : public StunAttribute {
 public:
  static const uint16 SIZE = 4;
  StunUInt32Attribute(uint16 type, uint32 value);
  explicit StunUInt32Attribute(uint16 type);

  virtual StunAttributeValueType value_type() const {
    return STUN_VALUE_UINT32;
  }

  uint32 value() const { return bits_; }
  void SetValue(uint32 bits) { bits_ = bits; }

  bool GetBit(size_t index) const;
  void SetBit(size_t index, bool value);

  virtual bool Read(talk_base::ByteBuffer* buf);
  virtual bool Write(talk_base::ByteBuffer* buf) const;

 private:
  uint32 bits_;
};

class StunUInt64Attribute : public StunAttribute {
 public:
  static const uint16 SIZE = 8;
  StunUInt64Attribute(uint16 type, uint64 value);
  explicit StunUInt64Attribute(uint16 type);

  virtual StunAttributeValueType value_type() const {
    return STUN_VALUE_UINT64;
  }

  uint64 value() const { return bits_; }
  void SetValue(uint64 bits) { bits_ = bits; }

  virtual bool Read(talk_base::ByteBuffer* buf);
  virtual bool Write(talk_base::ByteBuffer* buf) const;

 private:
  uint64 bits_;
};

// Implements STUN attributes that record an arbitrary byte string.
class StunByteStringAttribute : public StunAttribute {
 public:
  explicit StunByteStringAttribute(uint16 type);
  StunByteStringAttribute(uint16 type, const std::string& str);
  StunByteStringAttribute(uint16 type, const void* bytes, size_t length);
  StunByteStringAttribute(uint16 type, uint16 length);
  ~StunByteStringAttribute();

  virtual StunAttributeValueType value_type() const {
    return STUN_VALUE_BYTE_STRING;
  }

  const char* bytes() const { return bytes_; }
  std::string GetString() const { return std::string(bytes_, length()); }

  void CopyBytes(const char* bytes);  // uses strlen
  void CopyBytes(const void* bytes, size_t length);

  uint8 GetByte(size_t index) const;
  void SetByte(size_t index, uint8 value);

  virtual bool Read(talk_base::ByteBuffer* buf);
  virtual bool Write(talk_base::ByteBuffer* buf) const;

 private:
  void SetBytes(char* bytes, size_t length);

  char* bytes_;
};

// Implements STUN attributes that record an error code.
class StunErrorCodeAttribute : public StunAttribute {
 public:
  static const uint16 MIN_SIZE = 4;
  StunErrorCodeAttribute(uint16 type, int code, const std::string& reason);
  StunErrorCodeAttribute(uint16 type, uint16 length);
  ~StunErrorCodeAttribute();

  virtual StunAttributeValueType value_type() const {
    return STUN_VALUE_ERROR_CODE;
  }

  // The combined error and class, e.g. 0x400.
  int code() const;
  void SetCode(int code);

  // The individual error components.
  int eclass() const { return class_; }
  int number() const { return number_; }
  const std::string& reason() const { return reason_; }
  void SetClass(uint8 eclass) { class_ = eclass; }
  void SetNumber(uint8 number) { number_ = number; }
  void SetReason(const std::string& reason);

  bool Read(talk_base::ByteBuffer* buf);
  bool Write(talk_base::ByteBuffer* buf) const;

 private:
  uint8 class_;
  uint8 number_;
  std::string reason_;
};

// Implements STUN attributes that record a list of attribute names.
class StunUInt16ListAttribute : public StunAttribute {
 public:
  StunUInt16ListAttribute(uint16 type, uint16 length);
  ~StunUInt16ListAttribute();

  virtual StunAttributeValueType value_type() const {
    return STUN_VALUE_UINT16_LIST;
  }

  size_t Size() const;
  uint16 GetType(int index) const;
  void SetType(int index, uint16 value);
  void AddType(uint16 value);

  bool Read(talk_base::ByteBuffer* buf);
  bool Write(talk_base::ByteBuffer* buf) const;

 private:
  std::vector<uint16>* attr_types_;
};

// Returns the (successful) response type for the given request type.
// Returns -1 if |request_type| is not a valid request type.
int GetStunSuccessResponseType(int request_type);

// Returns the error response type for the given request type.
// Returns -1 if |request_type| is not a valid request type.
int GetStunErrorResponseType(int request_type);

// Returns whether a given message is a request type.
bool IsStunRequestType(int msg_type);

// Returns whether a given message is an indication type.
bool IsStunIndicationType(int msg_type);

// Returns whether a given response is a success type.
bool IsStunSuccessResponseType(int msg_type);

// Returns whether a given response is an error type.
bool IsStunErrorResponseType(int msg_type);

// Computes the STUN long-term credential hash.
bool ComputeStunCredentialHash(const std::string& username,
    const std::string& realm, const std::string& password, std::string* hash);

// TODO: Move the TURN/ICE stuff below out to separate files.
extern const char TURN_MAGIC_COOKIE_VALUE[4];

// "GTURN" STUN methods.
// TODO: Rename these methods to GTURN_ to make it clear they aren't
// part of standard STUN/TURN.
enum RelayMessageType {
  // For now, using the same defs from TurnMessageType below.
  // STUN_ALLOCATE_REQUEST              = 0x0003,
  // STUN_ALLOCATE_RESPONSE             = 0x0103,
  // STUN_ALLOCATE_ERROR_RESPONSE       = 0x0113,
  STUN_SEND_REQUEST                     = 0x0004,
  STUN_SEND_RESPONSE                    = 0x0104,
  STUN_SEND_ERROR_RESPONSE              = 0x0114,
  STUN_DATA_INDICATION                  = 0x0115,
};

// "GTURN"-specific STUN attributes.
// TODO: Rename these attributes to GTURN_ to avoid conflicts.
enum RelayAttributeType {
  STUN_ATTR_LIFETIME                    = 0x000d,  // UInt32
  STUN_ATTR_MAGIC_COOKIE                = 0x000f,  // ByteString, 4 bytes
  STUN_ATTR_BANDWIDTH                   = 0x0010,  // UInt32
  STUN_ATTR_DESTINATION_ADDRESS         = 0x0011,  // Address
  STUN_ATTR_SOURCE_ADDRESS2             = 0x0012,  // Address
  STUN_ATTR_DATA                        = 0x0013,  // ByteString
  STUN_ATTR_OPTIONS                     = 0x8001,  // UInt32
};

// A "GTURN" STUN message.
class RelayMessage : public StunMessage {
 protected:
  virtual StunAttributeValueType GetAttributeValueType(int type) const {
    switch (type) {
      case STUN_ATTR_LIFETIME:            return STUN_VALUE_UINT32;
      case STUN_ATTR_MAGIC_COOKIE:        return STUN_VALUE_BYTE_STRING;
      case STUN_ATTR_BANDWIDTH:           return STUN_VALUE_UINT32;
      case STUN_ATTR_DESTINATION_ADDRESS: return STUN_VALUE_ADDRESS;
      case STUN_ATTR_SOURCE_ADDRESS2:     return STUN_VALUE_ADDRESS;
      case STUN_ATTR_DATA:                return STUN_VALUE_BYTE_STRING;
      case STUN_ATTR_OPTIONS:             return STUN_VALUE_UINT32;
      default: return StunMessage::GetAttributeValueType(type);
    }
  }
  virtual StunMessage* CreateNew() const { return new RelayMessage(); }
};

// Defined in TURN RFC 5766.
enum TurnMessageType {
  STUN_ALLOCATE_REQUEST                 = 0x0003,
  STUN_ALLOCATE_RESPONSE                = 0x0103,
  STUN_ALLOCATE_ERROR_RESPONSE          = 0x0113,
  TURN_REFRESH_REQUEST                  = 0x0004,
  TURN_REFRESH_RESPONSE                 = 0x0104,
  TURN_REFRESH_ERROR_RESPONSE           = 0x0114,
  TURN_SEND_INDICATION                  = 0x0016,
  TURN_DATA_INDICATION                  = 0x0017,
  TURN_CREATE_PERMISSION_REQUEST        = 0x0008,
  TURN_CREATE_PERMISSION_RESPONSE       = 0x0108,
  TURN_CREATE_PERMISSION_ERROR_RESPONSE = 0x0118,
  TURN_CHANNEL_BIND_REQUEST             = 0x0009,
  TURN_CHANNEL_BIND_RESPONSE            = 0x0109,
  TURN_CHANNEL_BIND_ERROR_RESPONSE      = 0x0119,
};

enum TurnAttributeType {
  STUN_ATTR_CHANNEL_NUMBER              = 0x000C,  // UInt32
  STUN_ATTR_TURN_LIFETIME               = 0x000d,  // UInt32
  STUN_ATTR_XOR_PEER_ADDRESS            = 0x0012,  // XorAddress
  // TODO(mallinath) - Uncomment after RelayAttributes are renamed.
  // STUN_ATTR_DATA                     = 0x0013,  // ByteString
  STUN_ATTR_XOR_RELAYED_ADDRESS         = 0x0016,  // XorAddress
  STUN_ATTR_EVEN_PORT                   = 0x0018,  // ByteString, 1 byte.
  STUN_ATTR_REQUESTED_TRANSPORT         = 0x0019,  // UInt32
  STUN_ATTR_DONT_FRAGMENT               = 0x001A,  // No content, Length = 0
  STUN_ATTR_RESERVATION_TOKEN           = 0x0022,  // ByteString, 8 bytes.
  // TODO(mallinath) - Rename STUN_ATTR_TURN_LIFETIME to STUN_ATTR_LIFETIME and
  // STUN_ATTR_TURN_DATA to STUN_ATTR_DATA. Also rename RelayMessage attributes
  // by appending G to attribute name.
};

// RFC 5766-defined errors.
enum TurnErrorType {
  STUN_ERROR_FORBIDDEN                  = 403,
  STUN_ERROR_ALLOCATION_MISMATCH        = 437,
  STUN_ERROR_WRONG_CREDENTIALS          = 441,
  STUN_ERROR_UNSUPPORTED_PROTOCOL       = 442
};
extern const char STUN_ERROR_REASON_FORBIDDEN[];
extern const char STUN_ERROR_REASON_ALLOCATION_MISMATCH[];
extern const char STUN_ERROR_REASON_WRONG_CREDENTIALS[];
extern const char STUN_ERROR_REASON_UNSUPPORTED_PROTOCOL[];
class TurnMessage : public StunMessage {
 protected:
  virtual StunAttributeValueType GetAttributeValueType(int type) const {
    switch (type) {
      case STUN_ATTR_CHANNEL_NUMBER:      return STUN_VALUE_UINT32;
      case STUN_ATTR_TURN_LIFETIME:       return STUN_VALUE_UINT32;
      case STUN_ATTR_XOR_PEER_ADDRESS:    return STUN_VALUE_XOR_ADDRESS;
      case STUN_ATTR_DATA:                return STUN_VALUE_BYTE_STRING;
      case STUN_ATTR_XOR_RELAYED_ADDRESS: return STUN_VALUE_XOR_ADDRESS;
      case STUN_ATTR_EVEN_PORT:           return STUN_VALUE_BYTE_STRING;
      case STUN_ATTR_REQUESTED_TRANSPORT: return STUN_VALUE_UINT32;
      case STUN_ATTR_DONT_FRAGMENT:       return STUN_VALUE_BYTE_STRING;
      case STUN_ATTR_RESERVATION_TOKEN:   return STUN_VALUE_BYTE_STRING;
      default: return StunMessage::GetAttributeValueType(type);
    }
  }
  virtual StunMessage* CreateNew() const { return new TurnMessage(); }
};

// RFC 5245 ICE STUN attributes.
enum IceAttributeType {
  STUN_ATTR_PRIORITY                    = 0x0024,  // UInt32
  STUN_ATTR_USE_CANDIDATE               = 0x0025,  // No content, Length = 0
  STUN_ATTR_ICE_CONTROLLED              = 0x8029,  // UInt64
  STUN_ATTR_ICE_CONTROLLING             = 0x802A   // UInt64
};

// RFC 5245-defined errors.
enum IceErrorCode {
  STUN_ERROR_ROLE_CONFLICT              = 487,
};
extern const char STUN_ERROR_REASON_ROLE_CONFLICT[];

// A RFC 5245 ICE STUN message.
class IceMessage : public StunMessage {
 protected:
  virtual StunAttributeValueType GetAttributeValueType(int type) const {
    switch (type) {
      case STUN_ATTR_PRIORITY:        return STUN_VALUE_UINT32;
      case STUN_ATTR_USE_CANDIDATE:   return STUN_VALUE_BYTE_STRING;
      case STUN_ATTR_ICE_CONTROLLED:  return STUN_VALUE_UINT64;
      case STUN_ATTR_ICE_CONTROLLING: return STUN_VALUE_UINT64;
      default: return StunMessage::GetAttributeValueType(type);
    }
  }
  virtual StunMessage* CreateNew() const { return new IceMessage(); }
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_STUN_H_
