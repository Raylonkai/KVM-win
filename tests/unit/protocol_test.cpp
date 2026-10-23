#include "sharekeys/protocol.hpp"
#include "sharekeys/network.hpp"
#include <gtest/gtest.h>
using namespace sharekeys;
using namespace sharekeys::protocol;
TEST(Protocol, ExplicitNetworkByteOrderAndRoundtrip) {
    Frame frame{Type::KeyDown, 0x0102030405060708, 1, encodeKey({{0x1D, 0xA3, true}, true})};
    const auto bytes = encode(frame);
    EXPECT_EQ(bytes[3], 25);
    EXPECT_EQ(bytes[8], 1);
    EXPECT_EQ(bytes[15], 8);
    EXPECT_EQ(bodyLength(std::span(bytes).first(4)), 25U);
    const auto decoded = decode(std::span(bytes).subspan(4));
    const auto event = decodeKey(decoded);
    EXPECT_EQ(event.key.scan, 0x1D);
    EXPECT_EQ(event.key.vk, 0xA3);
    EXPECT_TRUE(event.key.extended);
    EXPECT_TRUE(event.down);
}
TEST(Protocol, MalformedAndTruncatedPacketsFailClosed) {
    auto bytes = encode({Type::Heartbeat, 1, 1, {}});
    for (std::size_t size = 0; size < header_size; ++size) EXPECT_THROW(decode(std::span(bytes).subspan(4, size)), std::runtime_error);
    bytes[5] = 2; EXPECT_THROW(decode(std::span(bytes).subspan(4)), std::runtime_error);
    EXPECT_THROW(bodyLength(std::vector<std::uint8_t>{255, 255, 255, 255}), std::runtime_error);
    EXPECT_THROW(encode((Frame{Type::KeyDown, 1, 1, {0, 1, 0, 0, 0}})), std::runtime_error);
    EXPECT_THROW(encode((Frame{Type::Heartbeat, 1, 1, {42}})), std::runtime_error);
    EXPECT_THROW(encode((Frame{static_cast<Type>(99), 1, 1, {}})), std::runtime_error);
}
TEST(Protocol, DuplicateMissingAndStaleSequenceRejected) {
    Sequence sequence;
    EXPECT_FALSE(sequence.accept(0));
    EXPECT_TRUE(sequence.accept(1));
    EXPECT_FALSE(sequence.accept(1));
    EXPECT_FALSE(sequence.accept(3));
    EXPECT_TRUE(sequence.accept(2));
}
TEST(NetworkPolicy, LanOnlyAndBoundedBackoff) {
    EXPECT_TRUE(privateAddress(asio::ip::make_address("192.168.1.1")));
    EXPECT_TRUE(privateAddress(asio::ip::make_address("172.31.1.1")));
    EXPECT_FALSE(privateAddress(asio::ip::make_address("172.32.1.1")));
    EXPECT_FALSE(privateAddress(asio::ip::make_address("8.8.8.8")));
    Backoff backoff;
    EXPECT_EQ(backoff.next().count(), 250);
    EXPECT_EQ(backoff.next().count(), 500);
    for (int i = 0; i < 20; ++i) EXPECT_LE(backoff.next().count(), 30000);
    backoff.reset(); EXPECT_EQ(backoff.next().count(), 250);
}
