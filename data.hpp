#ifndef _DATA_HPP
#define _DATA_HPP

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

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
// TODO(EliSauder): Change bodySize to packet# and add number of packets field

struct __attribute__((packed)) Packet {
    Header header{};
    uint8_t* data{};
    size_t dataLength{};

    ~Packet() { free(data); }
};

enum class RecievePacketErrors {
    NoPacketToRead = 0,
    UnableToReadHeader = 1,
    UnableToReadBody = 2,
    NoDataAfterHeader = 3,
    ExpectedPacketSizeDoesntMatchActual = 4,
    CallbackPreventedReadingBody = 5,
};

void recievePacket(int packetSize);

util::Result<Packet, RecievePacketErrors> recievePacket(
    LoRaClass& lora, int packetSize,
    bool (*shouldReadBody)(struct Header&) = nullptr);

bool sendMessage(LoRaClass& lora, const Packet& message);

edcom::util::Optional<struct Header> getHeader(LoRaClass& lora,
                                               int& packetSize);

enum WhatToDoWithPacket {
    Nothing = 0b0,
    Retransmit = 0b1,
    Handle = 0b10,
    ForwardToBridge = 0b100,
};

WhatToDoWithPacket whatToDoWithPacket(uint8_t currentDeviceId,
                                      HardwareSerial& serial,
                                      struct Packet& packet);

}  // namespace data

extern queue::Queue<data::Packet> IncomingMessages;

}  // namespace edcom

#endif
