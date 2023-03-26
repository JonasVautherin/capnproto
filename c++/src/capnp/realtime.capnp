# Copyright (c) 2022 Jonas Vautherin and contributors
# Licensed under the MIT License:
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

@0xb629d5204a572a0e;
# Defines placeholder types used to provide backwards-compatibility while introducing realtime
# streaming to the language. The goal is that old code generators that don't know about realtime
# streaming can still generate code that functions, though it won't be "realtime".

$import "/capnp/c++.capnp".namespace("capnp");

struct RealtimeResult @0xf1e925c2b9cb4a21 {
  # Empty struct that serves as the return type for "realtime streaming" methods.
  #
  # Defining a method like:
  #
  #     write @0 (bytes :Data) -> realtime stream;
  #
  # Is equivalent to:
  #
  #     write @0 (bytes :Data) -> import "/capnp/realtime.capnp".RealtimeResult;
  #
  # However, implementations that recognize realtime streaming will elide the reference to
  # RealtimeResult and instead give write() a different signature appropriate for realtime
  # streaming.
  #
  # Realtime streaming methods do not return a result -- that is, they return Promise<void>.
  # This promise resolves not to indicate that the call was actually delivered, but instead
  # that it was sent. Realtime calls indicate to the receiver does not need to respond, and
  # the caller does not mind if some calls are lost.
}
