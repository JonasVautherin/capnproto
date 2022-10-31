// Copyright (c) 2022 Jonas Vautherin and contributors
// Licensed under the MIT License:
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <capnp/compat/udp-rpc.h>
#include <kj/io.h>
#include <capnp/serialize.h>

namespace capnp {

UdpMessageStream::UdpMessageStream(kj::DatagramPort& datagramPort,
                                   kj::NetworkAddress& destination)
  : datagramPort(datagramPort),
    datagramReceiver(datagramPort.makeReceiver()),
    destination(destination)
  {}

kj::Promise<kj::Maybe<MessageReaderAndFds>> UdpMessageStream::tryReadMessage(
    kj::ArrayPtr<kj::AutoCloseFd> fdSpace,
    ReaderOptions options, kj::ArrayPtr<word> scratchSpace) {
  return datagramReceiver->receive()
      .then([this, options]() -> kj::Promise<kj::Maybe<MessageReaderAndFds>> {
        auto msg = datagramReceiver->getContent();
        // TODO msg.isTruncated? -> do something with that?
        auto bytes = msg.value;
        kj::Own<capnp::MessageReader> reader;
        size_t sizeInWords = bytes.size() / sizeof(word);
        if (reinterpret_cast<uintptr_t>(bytes.begin()) % alignof(word) == 0) {
          reader = kj::heap<FlatArrayMessageReader>(
              kj::arrayPtr(
                // TODO: const_cast!
                reinterpret_cast<word *>(const_cast<unsigned char *>(bytes.begin())),
                sizeInWords
              ),
              options).attach(kj::mv(bytes));
        } else {
          // The array is misaligned, so we need to copy it.
          auto words = kj::heapArray<word>(sizeInWords);

          // Note: can't just use bytes.size(), since the target buffer may
          // be shorter due to integer division.
          memcpy(words.begin(), bytes.begin(), sizeInWords * sizeof(word));
          reader = kj::heap<FlatArrayMessageReader>(
              kj::arrayPtr(words.begin(), sizeInWords),
              options).attach(kj::mv(words));
        }
        return kj::Maybe<MessageReaderAndFds>(MessageReaderAndFds {
          kj::mv(reader),
          nullptr
        });
      });
}

kj::Promise<void> UdpMessageStream::writeMessage(
    kj::ArrayPtr<const int> fds,
    kj::ArrayPtr<const kj::ArrayPtr<const word>> segments) {
  // TODO(perf): Right now the WebSocket interface only supports send() for
  // contiguous arrays, so we need to copy the whole message into a new buffer
  // in order to send it, whereas ideally we could just write each segment
  // (and the segment table) in sequence. Perhaps we should extend the WebSocket
  // interface to be able to send an ArrayPtr<ArrayPtr<byte>> as one binary
  // message, and then use that to avoid an extra copy here.

  auto stream = kj::heap<kj::VectorOutputStream>(
      computeSerializedSizeInWords(segments) * sizeof(word));
  capnp::writeMessage(*stream, segments);
  auto arrayPtr = stream->getArray();
  return datagramPort.send(arrayPtr.begin(), arrayPtr.size(), destination).attach(kj::mv(stream)).ignoreResult();
}

kj::Promise<void> UdpMessageStream::writeMessages(
    kj::ArrayPtr<kj::ArrayPtr<const kj::ArrayPtr<const word>>> messages) {
  // TODO(perf): Extend WebSocket interface with a way to write multiple messages at once.

  if(messages.size() == 0) {
    return kj::READY_NOW;
  }
  return writeMessage(nullptr, messages[0])
      .then([this, messages = messages.slice(1, messages.size())]() mutable -> kj::Promise<void> {
    return writeMessages(messages);
  });
}

kj::Maybe<int> UdpMessageStream::getSendBufferSize() {
  return nullptr;
}

kj::Promise<void> UdpMessageStream::end() {
  return kj::READY_NOW;
}

}
