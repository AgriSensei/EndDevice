#ifndef _DATA_HPP
#define _DATA_HPP

#include <stddef.h>
#include <stdint.h>

#include "queue.hpp"
#include "util.hpp"

namespace edcom {
namespace data {

enum class IdType {
    Bridge = 0,
    AnyEndDevice = 1,
    SpecificEndDevice = 2,
};

struct __attribute__((packed)) Header {
    IdType fromType{};
    edcom::util::Optional<uint16_t> fromId{};
    IdType toType{};
    edcom::util::Optional<uint16_t> toId{};
    uint8_t messageId{};
    uint8_t messageType{};
    uint16_t bodySize{};
};

struct __attribute__((packed)) Packet {
    Header header{};
    uint8_t* data{};
    size_t dataLength{};
};

enum class RecievePacketErrors {
    NoPacketToRead = 0b0,
    UnableToReadHeader = 0b1,
    UnableToReadBody = 0b10,
    NoDataAfterHeader = 0b100,
    ExpectedPacketSizeDoesntMatchActual = 0b1000,
};

util::Result<Packet, RecievePacketErrors> recievePacket(LoRaClass& lora,
                                                        int packetSize);

bool sendMessage(const Packet& message);

edcom::util::Optional<struct Header> getHeader(LoRaClass& lora,
                                               int& packetSize);

}  // namespace data

extern queue::Queue<data::Packet> IncomingMessages;

}  // namespace edcom

#endif
