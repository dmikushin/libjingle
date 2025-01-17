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

#ifndef TALK_P2P_BASE_SESSIONDESCRIPTION_H_
#define TALK_P2P_BASE_SESSIONDESCRIPTION_H_

#include <string>
#include <vector>

#include "base/constructormagic.h"
#include "p2p/base/transportinfo.h"

namespace cricket {

// Describes a session content. Individual content types inherit from
// this class.  Analagous to a <jingle><content><description> or
// <session><description>.
class ContentDescription {
 public:
  virtual ~ContentDescription() {}
  virtual ContentDescription* Copy() const = 0;
};

// Analagous to a <jingle><content> or <session><description>.
// name = name of <content name="...">
// type = xmlns of <content>
struct ContentInfo {
  ContentInfo() : description(NULL) {}
  ContentInfo(const std::string& name,
              const std::string& type,
              ContentDescription* description) :
      name(name), type(type), rejected(false), description(description) {}
  ContentInfo(const std::string& name,
              const std::string& type,
              bool rejected,
              ContentDescription* description) :
      name(name), type(type), rejected(rejected), description(description) {}
  std::string name;
  std::string type;
  bool rejected;
  ContentDescription* description;
};

typedef std::vector<std::string> ContentNames;

// This class provides a mechanism to aggregate different media contents into a
// group. This group can also be shared with the peers in a pre-defined format.
// GroupInfo should be populated only with the |content_name| of the
// MediaDescription.
class ContentGroup {
 public:
  explicit ContentGroup(const std::string& semantics) :
      semantics_(semantics) {}

  const std::string& semantics() const { return semantics_; }
  const ContentNames& content_names() const { return content_names_; }

  const std::string* FirstContentName() const;
  bool HasContentName(const std::string& content_name) const;
  void AddContentName(const std::string& content_name);
  bool RemoveContentName(const std::string& content_name);

 private:
  std::string semantics_;
  ContentNames content_names_;
};

typedef std::vector<ContentInfo> ContentInfos;
typedef std::vector<ContentGroup> ContentGroups;

const ContentInfo* FindContentInfoByName(
    const ContentInfos& contents, const std::string& name);
const ContentInfo* FindContentInfoByType(
    const ContentInfos& contents, const std::string& type);

// Describes a collection of contents, each with its own name and
// type.  Analogous to a <jingle> or <session> stanza.  Assumes that
// contents are unique be name, but doesn't enforce that.
class SessionDescription {
 public:
  SessionDescription() {}
  explicit SessionDescription(const ContentInfos& contents) :
      contents_(contents) {}
  SessionDescription(const ContentInfos& contents,
                     const ContentGroups& groups) :
      contents_(contents),
      content_groups_(groups) {}
  SessionDescription(const ContentInfos& contents,
                     const TransportInfos& transports,
                     const ContentGroups& groups) :
      contents_(contents),
      transport_infos_(transports),
      content_groups_(groups) {}
  ~SessionDescription() {
    for (ContentInfos::iterator content = contents_.begin();
         content != contents_.end(); ++content) {
      delete content->description;
    }
  }

  SessionDescription* Copy() const;

  // Content accessors.
  const ContentInfos& contents() const { return contents_; }
  ContentInfos& contents() { return contents_; }
  const ContentInfo* GetContentByName(const std::string& name) const;
  ContentInfo* GetContentByName(const std::string& name);
  const ContentDescription* GetContentDescriptionByName(
      const std::string& name) const;
  ContentDescription* GetContentDescriptionByName(const std::string& name);
  const ContentInfo* FirstContentByType(const std::string& type) const;
  const ContentInfo* FirstContent() const;

  // Content mutators.
  // Adds a content to this description. Takes ownership of ContentDescription*.
  void AddContent(const std::string& name,
                  const std::string& type,
                  ContentDescription* description);
  void AddContent(const std::string& name,
                  const std::string& type,
                  bool rejected,
                  ContentDescription* description);
  bool RemoveContentByName(const std::string& name);

  // Transport accessors.
  const TransportInfos& transport_infos() const { return transport_infos_; }
  TransportInfos& transport_infos() { return transport_infos_; }
  const TransportInfo* GetTransportInfoByName(
      const std::string& name) const;
  const TransportDescription* GetTransportDescriptionByName(
      const std::string& name) const {
    const TransportInfo* tinfo = GetTransportInfoByName(name);
    return tinfo ? &tinfo->description : NULL;
  }

  // Transport mutators.
  void set_transport_infos(const TransportInfos& transport_infos) {
    transport_infos_ = transport_infos;
  }
  // Adds a TransportInfo to this description.
  // Returns false if a TransportInfo with the same name already exists.
  bool AddTransportInfo(const TransportInfo& transport_info);
  bool RemoveTransportInfoByName(const std::string& name);

  // Group accessors.
  const ContentGroups& groups() const { return content_groups_; }
  const ContentGroup* GetGroupByName(const std::string& name) const;
  bool HasGroup(const std::string& name) const;

  // Group mutators.
  void AddGroup(const ContentGroup& group) { content_groups_.push_back(group); }
  // Remove the first group with the same semantics specified by |name|.
  void RemoveGroupByName(const std::string& name);

 private:
  ContentInfos contents_;
  TransportInfos transport_infos_;
  ContentGroups content_groups_;
};

// Indicates whether a ContentDescription was an offer or an answer, as
// described in http://www.ietf.org/rfc/rfc3264.txt. CA_UPDATE
// indicates a jingle update message which contains a subset of a full
// session description
enum ContentAction {
  CA_OFFER, CA_PRANSWER, CA_ANSWER, CA_UPDATE
};

// Indicates whether a ContentDescription was sent by the local client
// or received from the remote client.
enum ContentSource {
  CS_LOCAL, CS_REMOTE
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_SESSIONDESCRIPTION_H_
