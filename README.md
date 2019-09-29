# Libjingle

## Introduction

Libjingle is a set of components provided by Google to implement Jingle
protocols XEP-166 (http://xmpp.org/extensions/xep-0166.html) and XEP-167
(http://xmpp.org/extensions/xep-0167.html). Libjingle is also backward
compatible with
[Google Talk Call Signaling](http://code.google.com/apis/talk/call_signaling.html). This package will
create several static libraries you may link to your projects as needed.

```
|-base              - Contains basic low-level portable utility functions for
|                     things like threads and sockets
|-p2p               - The P2P stack
  |-base            - Base p2p functionality
  |-client          - Hooks to tie it into XMPP
|-session           - Signaling
  |-phone           - Signaling code specific to making phone calls
    |-testdata      - Samples of RTP voice and video dump
  |-tunnel          - Tunnel session and channel
|-third_party       - Folder for third party libraries
  |-libudev         - Folder containing libudev.h
|-xmllite           - XML parser
|-xmpp              - XMPP engine
```

In addition, this package contains two examples in talk/examples which
illustrate the basic concepts of how the provided classes work.

## Building

Prerequisites:

```
sudo apt install g++ cmake libnss3-dev libasound2-dev gtk+2.0 libexpat-dev libsrtp-dev libnspr4-dev
```

Building from source with CMake:

```
git clone --recursive https://github.com/dmikushin/libjingle.git
cd libjingle
mkdir build
cmake ..
make -j48
```

## Testing

The source code comes with two major client application examples: `examples/login` and `examples/call`. In order to test them, you first need an XMPP server, e.g. prosody:

```
sudo apt install prosody
```

Create test users in prosody server:

```
sudo prosodyctl register robot user simplepassword
sudo prosodyctl register robot localhost simplepassword
sudo prosodyctl restart
```

Now a call example can connect to the server:

```
cd build/
examples/jingle_example_call -s localhost
```

Type in `robot@localhost` and `simplepassword` when prompted.

Execute another instance of test with a different username: `user@localhost`

In both instances of `jingle_example_call` type `join test@conference.localhost`. Both users shall be joined to the new `test` chart room. On behalf of `user` send a message to `robot` using `send` command:

```
send robot@localhost Hello
```

The username is cached, so subsequent messages can be sent with `send` omitting the username.

## Further testing

For the call sample, you can specify the input and
output RTP dump for voice and video. This package provides two samples of input
RTP dump: voice.rtpdump is a single channel, 16Khz voice encoded with G722, and
video.rtpdump is 320x240 video encoded with H264 AVC at 30 frames per second.
These provided samples will inter-operate with Google Talk Video. If you use
other input RTP dump, you may need to change the codecs in `call_main.cc`, lines
215 - 222.

Libjingle also builds two server tools, a relay server and a STUN server. The
relay server may be used to relay traffic when a direct peer-to-peer connection
could not be established. The STUN Server implements the STUN protocol for
Session Traversal Utilities for NAT(rfc5389), and the TURN server is in active
development to reach compatibility with rfc5766. See the
[Libjingle Developer Guide](http://developers.google.com/talk/libjingle/developer_guide) for
information about configuring a client to use this relay server and this STUN
server.

## LinphoneMediaEngine

To use LinphoneMediaEngine, you need to perform the following additional steps:
  * Download and install the "MediaStreamer" library on your
    machine.
  * Add the following lines into the libjingle.scons file.
    In the "talk.Library(env, name = "libjingle",..." section, you need to add:
      "HAVE_LINPHONE",
      "HAVE_SPEEX",
      "HAVE_ILBC",
    to the "cppdefines = [".

    In the "talk.App(env, name = "call",..." section, you need to add:
      "mediastreamer",
    to the "libs = [".
  * In the libjingle.scons file, add the following line into the "srcs = [ ..."
    section of the "libjingle" Library.
      "session/phone/linphonemediaengine.cc",

