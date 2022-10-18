// Copyright (c) 2013-2014 Sandstorm Development Group, Inc. and contributors
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

#define CAPNP_TESTING_CAPNP 1

#include "ez-rpc.h"
#include "test-util.h"
#include <kj/compat/gtest.h>

//#if KJ_HAS_OPENSSL
#include <kj/compat/tls.h>
//#endif

namespace capnp {
namespace _ {
namespace {

TEST(EzRpc, Basic) {
  int callCount = 0;
  EzRpcServer server(kj::heap<TestInterfaceImpl>(callCount), "localhost");

  EzRpcClient client("localhost", server.getPort().wait(server.getWaitScope()));

  auto cap = client.getMain<test::TestInterface>();
  auto request = cap.fooRequest();
  request.setI(123);
  request.setJ(true);

  EXPECT_EQ(0, callCount);
  auto response = request.send().wait(server.getWaitScope());
  EXPECT_EQ("foo", response.getX());
  EXPECT_EQ(1, callCount);
}

TEST(EzRpc, DeprecatedNames) {
  EzRpcServer server("localhost");
  int callCount = 0;
  server.exportCap("cap1", kj::heap<TestInterfaceImpl>(callCount));
  server.exportCap("cap2", kj::heap<TestCallOrderImpl>());

  EzRpcClient client("localhost", server.getPort().wait(server.getWaitScope()));

  auto cap = client.importCap<test::TestInterface>("cap1");
  auto request = cap.fooRequest();
  request.setI(123);
  request.setJ(true);

  EXPECT_EQ(0, callCount);
  auto response = request.send().wait(server.getWaitScope());
  EXPECT_EQ("foo", response.getX());
  EXPECT_EQ(1, callCount);

  EXPECT_EQ(0, client.importCap("cap2").castAs<test::TestCallOrder>()
      .getCallSequenceRequest().send().wait(server.getWaitScope()).getN());
  EXPECT_EQ(1, client.importCap("cap2").castAs<test::TestCallOrder>()
      .getCallSequenceRequest().send().wait(server.getWaitScope()).getN());
}

TEST(EzRpc, ExternalTcpMessageStream) {
  // Prepare server side (EzRpcServer + server stream)
  int callCount = 0;
  EzRpcServer server(kj::heap<TestInterfaceImpl>(callCount));
  auto& waitScope = server.getWaitScope();
  auto& serverIoProvider = server.getIoProvider();

  auto serverAddr = serverIoProvider.getNetwork().parseAddress("localhost", 0).wait(waitScope);
  auto connection = serverAddr->listen();
  auto serverPort = connection->getPort();
  auto acceptPromise = connection->accept().then([&server](kj::Own<kj::AsyncIoStream>&& stream) {
    server.addStream(kj::heap<AsyncIoMessageStream>(kj::mv(stream)), ReaderOptions());
  }).eagerlyEvaluate([](kj::Exception&& e) {
    KJ_LOG(ERROR, e.getDescription());
  });

  // Prepare client side (EzRpcClient + client stream)
  EzRpcClient client;
  auto& clientIoProvider = client.getIoProvider();

  auto clientAddr = clientIoProvider.getNetwork().parseAddress("127.0.0.1", serverPort).wait(waitScope);
  auto clientStream = clientAddr->connect().wait(waitScope);
  client.setStream(kj::heap<AsyncIoMessageStream>(kj::mv(clientStream)));

  auto cap = client.getMain<test::TestInterface>();
  auto request = cap.fooRequest();
  request.setI(123);
  request.setJ(true);

  EXPECT_EQ(0, callCount);
  auto response = request.send().wait(waitScope);
  EXPECT_EQ("foo", response.getX());
  EXPECT_EQ(1, callCount); //auto clientAddr = serverIoProvider.get
}

//#if KJ_HAS_OPENSSL

// test data
//
// made with make-test-certs.sh
static constexpr char CA_CERT[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIGMzCCBBugAwIBAgIUDxGXACZeJ0byrswV8gyWskZF2Q8wDQYJKoZIhvcNAQEL\n"
    "BQAwgacxCzAJBgNVBAYTAlVTMRMwEQYDVQQIDApDYWxpZm9ybmlhMRIwEAYDVQQH\n"
    "DAlQYWxvIEFsdG8xFTATBgNVBAoMDFNhbmRzdG9ybS5pbzEbMBkGA1UECwwSVGVz\n"
    "dGluZyBEZXBhcnRtZW50MRcwFQYDVQQDDA5jYS5leGFtcGxlLmNvbTEiMCAGCSqG\n"
    "SIb3DQEJARYTZ2FycGx5QHNhbmRzdG9ybS5pbzAgFw0yMDA2MjcwMDQyNTJaGA8y\n"
    "MTIwMDYwMzAwNDI1MlowgacxCzAJBgNVBAYTAlVTMRMwEQYDVQQIDApDYWxpZm9y\n"
    "bmlhMRIwEAYDVQQHDAlQYWxvIEFsdG8xFTATBgNVBAoMDFNhbmRzdG9ybS5pbzEb\n"
    "MBkGA1UECwwSVGVzdGluZyBEZXBhcnRtZW50MRcwFQYDVQQDDA5jYS5leGFtcGxl\n"
    "LmNvbTEiMCAGCSqGSIb3DQEJARYTZ2FycGx5QHNhbmRzdG9ybS5pbzCCAiIwDQYJ\n"
    "KoZIhvcNAQEBBQADggIPADCCAgoCggIBAKpp8VF/WDPw1V1aD36/uDWI4XRk9OaJ\n"
    "i8tkAbTPutJ7NU4AWv9OzreKIR1PPhj0DtxVOj5KYRTwL1r4DsFWh6D0rBV7oz7o\n"
    "zP8hWznVQBSa2BJ2E4uDD8p5oNz1+O+o4UgSBbOr83Gp5SZGw9KO7cgNql9Id/Ii\n"
    "sHYxXdrYdAl6InuR6q52CJgcGqQgpFYG+KYqDiByfX52slyz5FA/dfZxsmoEVFLB\n"
    "rgbeuhsJGIoasTkGIdCYJhYI7k2uWtvYNurnhgvlpfPHHuSnJ+aWVdKaQUthgbsy\n"
    "T2XHuLYpWx7+B7kCF5B4eKtn3e7yzE7A8jn8Teq6yRNUh1PnM7CRMmz3g4UAxmJT\n"
    "F5NyQd14IqveFuOk40Ba8wLoypll5We5tV7StUyvaOlAhi+gGPHfWKk4MGiBoaOV\n"
    "52D1+/dkh/abokdKZtE59gJX0MrH6mihfc9KQs7N51IhSs4kG5zGIBdvgXtmP17H\n"
    "hixUFi0Y85pqGidW4LLQ1pmK9k0U4gYlwHtqHh8an35/vp/mFhl2BDHcpuYKZ34U\n"
    "ZDo9GglfCTVEsUvAnwfhuYN0e0kveSTuRCMltjGg0Fs1h9ljNNuc46W4qIx/d5ls\n"
    "aMOTKc3PTtwtqgXOXRFn2U7AUXOgtEqyqpuj5ZcjH2YQ3BL24qAiYEHaBOHM+8qF\n"
    "9JLZE64j5dnZAgMBAAGjUzBRMB0GA1UdDgQWBBTmqsbDUpi5hgPbcPESYR9t8jsD\n"
    "7jAfBgNVHSMEGDAWgBTmqsbDUpi5hgPbcPESYR9t8jsD7jAPBgNVHRMBAf8EBTAD\n"
    "AQH/MA0GCSqGSIb3DQEBCwUAA4ICAQADdVBYClYWqNk1s2gamjGsyQ2r88TWTD6X\n"
    "RySVnyQPATWuEctrr6+8qTrbqBP4bTPKE+uTzwk+o5SirdJJAkrcwEsSCFw7J/qf\n"
    "5U/mXN+EUuqyiMHOS/vLe5X1enj0I6fqJY2mCGFD7Jr/+el1XXjzRZsLZHmqSxww\n"
    "T+UjJP+ffvtq3pq4nMQowxXm+Wub0gFHj5wkKMTIDyqnbjzB9bdVd0crtA+EpYIi\n"
    "f8y5WB46g1CngRnMzRQvg5FCmxg57i+mVgiUjUe54VenwK9aeeHIuOdLCZ0RmiNH\n"
    "KHPUBct+S/AXx8DCoAdm51EahwMBnlUyISpwJ+LVMWA2R9DOxdhEF0tv5iBsD9rn\n"
    "oKIWoa0t/Vwnd2n8wyLhuA7N4yzm0rdBjO/rU6n0atIab5+CEDyLeyWQBVwfCUF5\n"
    "XYNxOBJgGfSgJa23KUtn15pS/nSTa6sOtS/Mryc4UuNzxn+3ebNOG4UPlH6miSMK\n"
    "yA+5SCyKgrn3idifzrq+XafA2WUnxdBLgJMM4OIPAGNjCCW2P1cP/NVllUTjTy2y\n"
    "AIKQ/D9V/DzlbIIT6F3CNnqa9xnrBWTKF1YH/zSB7Gh2xlr0WnOWJVQbNUYet982\n"
    "JL5ibRhsiqBgltgQPhKhN/rGuh7Cb28679fQqLXKgOWvV2fC4b2y0v9dG78jGCEE\n"
    "LBzBUUmunw==\n"
    "-----END CERTIFICATE-----\n";

static constexpr char INTERMEDIATE_CERT[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIGETCCA/mgAwIBAgICEAAwDQYJKoZIhvcNAQELBQAwgacxCzAJBgNVBAYTAlVT\n"
    "MRMwEQYDVQQIDApDYWxpZm9ybmlhMRIwEAYDVQQHDAlQYWxvIEFsdG8xFTATBgNV\n"
    "BAoMDFNhbmRzdG9ybS5pbzEbMBkGA1UECwwSVGVzdGluZyBEZXBhcnRtZW50MRcw\n"
    "FQYDVQQDDA5jYS5leGFtcGxlLmNvbTEiMCAGCSqGSIb3DQEJARYTZ2FycGx5QHNh\n"
    "bmRzdG9ybS5pbzAgFw0yMDA2MjcwMDQyNTNaGA8yMTIwMDYwMzAwNDI1M1owgZcx\n"
    "CzAJBgNVBAYTAlVTMRMwEQYDVQQIDApDYWxpZm9ybmlhMRUwEwYDVQQKDAxTYW5k\n"
    "c3Rvcm0uaW8xGzAZBgNVBAsMElRlc3RpbmcgRGVwYXJ0bWVudDEbMBkGA1UEAwwS\n"
    "aW50LWNhLmV4YW1wbGUuY29tMSIwIAYJKoZIhvcNAQkBFhNnYXJwbHlAc2FuZHN0\n"
    "b3JtLmlvMIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAvDRmV4e7F/1K\n"
    "TaySC2xq00aYZfKJmwcvBvLM60xMrwDf5KlHWoYzog72q6qNb0TpvaeOlymU1IfR\n"
    "DkRIMFz6qd4QpK3Ah0vDtqEsoYE6F7gr2QAAaB7HQhczClh6FFkMCygrHDRQlJcF\n"
    "VseTKxnZBUAUhnG7OI8bHMZsprg31SNG1GCXq/CO/rkKIP1Pdwevr8DFHL3LFF03\n"
    "vdeo6+f/3LjlsCzVCNsCcAYIScRSl1scj2QYcwP7tTmDRAmU9EZFv9MWSqRIMT4L\n"
    "/4tP9AL/pWGdx8RRvbXoLJQf+hQW6YnSorRmIH/xYsvqMaan4P0hkomfIHP4nMYa\n"
    "LgI8VsNhTeDZ7IvSF2F73baluTOHhUD5eE/WDffNeslaCoMbH/B3H6ks0zYt/mHG\n"
    "mDaw3OxgMYep7TIE+SABOSJV+pbtQWNyM7u2+TYHm2DaxD3quf+BoYUZT01uDtN4\n"
    "BSsR7XEzF25w/4lDxqBxGAZ0DzItK0kzqMykSWvDIjpSg/UjRj05sc+5zcgE4pX1\n"
    "nOLD+FuB9jVqo6zCiIkHsSI0XHnm4D6awB1UyDwSh8mivfUDT53OpOIwOI3EB/4U\n"
    "iZstUKgyXNrXsE6wS/3JfdDZ9xkw4dWV+0FWKJ4W6Y8UgKvQJRChpCUtcuxfaLjX\n"
    "/ZIcMRYEFNjFppka/7frNT1VNnRvJ9cCAwEAAaNTMFEwHQYDVR0OBBYEFNnHDzWZ\n"
    "NC5pP6njUnf1BMXuQKnnMB8GA1UdIwQYMBaAFOaqxsNSmLmGA9tw8RJhH23yOwPu\n"
    "MA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQELBQADggIBAG0ShYBIbwGMDUBj\n"
    "9G2vxI4Nx7lip9/wO5NCC2biVxtcjNxwTuCcqCxeSGgdimo82vJTm9Wa1AkoawSv\n"
    "+spoXJAeOL5dGmZX3IRD0KuDoabH7F6xbNOG3HphvwAJcKQOitPAgTt2eGXCaBqw\n"
    "a1ivxPzchHVd3+LyjLMK5G3zzgW0tZhKp7IYE0eGoSGwrw10ox6ibyswe8evul1N\n"
    "r7Z3zFSWqjy6mcyp7PvlImDMYzWHYYdncEaeBHd13TnTQ++AtY3Da2lu7WG5VlcV\n"
    "vcRaNx8ZLIUpW7u76F9fH25GFnQdF87dgt/ufntts4NifwRRRdIDAgunQ0Nf/TDy\n"
    "ow/P7dobloGl9c5xGyXk9EbYYiK/Iuga1yUKJP3UQhfOTNBsDkyUD/jLg+0bI8T+\n"
    "Vv4hKNhzUObXjzA5P7+RoEqnPe8R5wjvea6OJc2MDm5GFrzZOdIEnW/iuaQTE3ol\n"
    "PYZVDvgPB+l7IC0brwTvdnvXaGLFu8ICQtNoOuSkbEAysyBWCEGwloYLL3r/0ozb\n"
    "k3z5mUZVBwhpQS/GEChfUKiLxk9rbDmnhKASa6FVtycSVfoW/Oh5M65ARf9CFIUv\n"
    "cvDYLpQl7Ao8QiDqlQBvitNi665uucsZ/zOdFlTKKAellLASpB/q2Y6J4wkogFNa\n"
    "dGRV0WpQkcuMjXK7bzSSLMo0nMCm\n"
    "-----END CERTIFICATE-----\n";

static constexpr char HOST_KEY[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "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\n"
    "-----END RSA PRIVATE KEY-----\n";

static constexpr char VALID_CERT[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIF+jCCA+KgAwIBAgICEAAwDQYJKoZIhvcNAQELBQAwgZcxCzAJBgNVBAYTAlVT\n"
    "MRMwEQYDVQQIDApDYWxpZm9ybmlhMRUwEwYDVQQKDAxTYW5kc3Rvcm0uaW8xGzAZ\n"
    "BgNVBAsMElRlc3RpbmcgRGVwYXJ0bWVudDEbMBkGA1UEAwwSaW50LWNhLmV4YW1w\n"
    "bGUuY29tMSIwIAYJKoZIhvcNAQkBFhNnYXJwbHlAc2FuZHN0b3JtLmlvMCAXDTIw\n"
    "MDYyNzAwNDI1M1oYDzIxMjAwNjI3MDA0MjUzWjCBkDELMAkGA1UEBhMCVVMxEzAR\n"
    "BgNVBAgMCkNhbGlmb3JuaWExFTATBgNVBAoMDFNhbmRzdG9ybS5pbzEbMBkGA1UE\n"
    "CwwSVGVzdGluZyBEZXBhcnRtZW50MRQwEgYDVQQDDAtleGFtcGxlLmNvbTEiMCAG\n"
    "CSqGSIb3DQEJARYTZ2FycGx5QHNhbmRzdG9ybS5pbzCCAiIwDQYJKoZIhvcNAQEB\n"
    "BQADggIPADCCAgoCggIBAMzuvxxieOjyKIGST8X0hOnh7qhNSbwvHmkgjiqam3xr\n"
    "CBz34fHfkxWrQ3uBheAemGGJsaSVDfOcGQvuhkwO8XjPVLm7LIsHwAhP7HuZSQNE\n"
    "km16PUl5BObCPyzOzQOvodz05qC5cHblh4ercB23H3CVLUjkiXpgi1KmTokkPghl\n"
    "tkcxBdNUjP3jpqTfYLZlXow+8DsSLW41/wwxPyCr+sUNhtUWMRKYIApzUI2Y+EdL\n"
    "q6Dei7TO4YkHm7nl7Nyn1SBHBFfaF3039rSOBtzU5/TCVL2UDSDFFTrY5fFJix35\n"
    "N0/R3Hi/0TNLL1iD1jaFCcQOpuLuIPQ9HhA9ltHISnvOrReuUUWB7zzUF2X/rRu8\n"
    "Mr0Sj3j1iJRMyAIF1tlsFxL8xb70Z319ZmHlgOyockWlXAqfR9IuWT9C/BNkbL2H\n"
    "ftkDJs68ZHjNNSjCUkwLVIasnTWh+Xc5afFF1r6ExSwIeBe+ODo3Tsox6qE2YtaI\n"
    "foqrHN6OLVkBd2M6Y4Wqz95p4Bo/mA6f2TehfEHRUFPojRSsemg0YJypvq4tP8kw\n"
    "zENxteG0PRTZi0cAfluVCPTsvd9SccXFuy8Z3Gx4AN9uvvmw9QnS18sTr6RyuNAe\n"
    "X/c7IJvfNMXYwXC69qNJxgLGfVe/WtATao7wwNw45lwRk2MSUqTBn4qT+Smz9oZd\n"
    "AgMBAAGjUzBRMB0GA1UdDgQWBBRdpPVLjMnJFs7lCtMW6x39Wnwt3TAfBgNVHSME\n"
    "GDAWgBTZxw81mTQuaT+p41J39QTF7kCp5zAPBgNVHRMBAf8EBTADAQH/MA0GCSqG\n"
    "SIb3DQEBCwUAA4ICAQA2kid2fGqjeDRVuclfDRr0LhbFYfJJXxW7SPgcUpJYXeAz\n"
    "LXotBm/Cc+K01nNtl0JYfJy4IkaQUYgfVsA5/FqTGnbRmpEd5XidiGE6PfkXZSNj\n"
    "02v6Uv2bAs8NnJirS5F0JhWZ45xAOMbl04QPUdkISF1JzioCcCqWOggbIV7kzwrB\n"
    "MTcsx8vuh04S9vB318pKli4uIjNdwu7HnoqbrqhSgTUK1aXS6sDQNN/nvR6F5TRL\n"
    "MC0cCtIA6n04c09WRfHxl/YPQwayxGD23eQ9UC7Noe/R8B3K/+6XUYIEmXx6HpnP\n"
    "yt/79iBPwLnaVjGDwKfI8EPuSo7AkDSO3uxMjf7eCL2sCzWlgsne9yOfYxGn+q9K\n"
    "h3KTOR1b7EVU/G4h7JxlSHqf3Ii9qFba/HsUo1yMjVEraMNpxXCijGsN30fqUpLg\n"
    "2g9lNKmIdyHuYdlZET082b1dvb7cfYChlqHvrPv5awbsc1Ka2pOFOMwnUi2w3cHc\n"
    "TLq3SyipI+sgJg3HHSJ3zVHrgKUeDoQi52i5WedvIBPfHC4Ik4zEzlkCMDd3hoLe\n"
    "THAsBGBBEICK+lLSo0Mst2kxhTHHK+PxmhorXXnGOpla2wbwzVZgxur45axCWVqH\n"
    "cbdcVhzR2w4XhjaS74WGiHHn5mHb/uYZJiZGpFCNefU2WOBlRCX7hgAzlPa4ZQ==\n"
    "-----END CERTIFICATE-----\n";

static kj::TlsContext::Options defaultClient() {
  static kj::TlsCertificate caCert(CA_CERT);
  kj::TlsContext::Options options;
  options.useSystemTrustStore = false;
  options.trustedCertificates = kj::arrayPtr(&caCert, 1);
  return options;
}

static kj::TlsContext::Options defaultServerOptions() {
  static kj::TlsKeypair keypair = {
      kj::TlsPrivateKey(HOST_KEY),
      kj::TlsCertificate(kj::str(VALID_CERT, INTERMEDIATE_CERT))};
  kj::TlsContext::Options options;
  options.defaultKeypair = keypair;
  return options;
}

TEST(EzRpc, ExternalTlsMessageStream) {
  // Prepare server side (EzRpcServer + server stream)
  int callCount = 0;
  EzRpcServer server(kj::heap<TestInterfaceImpl>(callCount));
  auto& waitScope = server.getWaitScope();
  auto& serverIoProvider = server.getIoProvider();

  KJ_DBG("SPARTA1");
  auto serverTlsOptions = defaultServerOptions();
  auto serverTlsContext = kj::heap<kj::TlsContext>(kj::mv(serverTlsOptions));

  KJ_DBG("SPARTA2");
  auto network = serverTlsContext->wrapNetwork(serverIoProvider.getNetwork());
  auto serverAddr = network->parseAddress("localhost", 0).wait(waitScope);
  auto connection = serverAddr->listen();
  auto serverPort = connection->getPort();
  auto acceptPromise = connection->accept().then([&server](kj::Own<kj::AsyncIoStream>&& stream) {
    server.addStream(kj::heap<AsyncIoMessageStream>(kj::mv(stream)), ReaderOptions());
  }).eagerlyEvaluate([](kj::Exception&& e) {
    KJ_LOG(ERROR, e.getDescription());
  });

  KJ_DBG("SPARTA3");
  // Prepare client side (EzRpcClient + client stream)
  EzRpcClient client;
  auto& clientIoProvider = client.getIoProvider();

  KJ_DBG("SPARTA4");
  kj::TlsContext clientTlsContext(defaultClient());
  KJ_DBG("SPARTA5");
  auto clientAddr = clientIoProvider.getNetwork().parseAddress("127.0.0.1", serverPort).wait(waitScope);
  KJ_DBG("SPARTA6");
  auto clientPlaintextStream = clientAddr->connect().wait(waitScope);
  KJ_DBG("SPARTA9");
  auto clientEncryptedStream =
      clientTlsContext.wrapClient(kj::mv(clientPlaintextStream), "example.com").wait(client.getWaitScope());
  KJ_DBG("SPARTA10");
  client.setStream(kj::heap<AsyncIoMessageStream>(kj::mv(clientEncryptedStream)));

  KJ_DBG("SPARTA5");
  auto cap = client.getMain<test::TestInterface>();
  auto request = cap.fooRequest();
  request.setI(123);
  request.setJ(true);

  EXPECT_EQ(0, callCount);
  auto response = request.send().wait(waitScope);
  EXPECT_EQ("foo", response.getX());
  EXPECT_EQ(1, callCount); //auto clientAddr = serverIoProvider.get
}

//#endif

}  // namespace
}  // namespace _
}  // namespace capnp
