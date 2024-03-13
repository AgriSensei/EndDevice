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

struct Header {
    IdType fromType{};
    edcom::util::Optional<uint16_t> fromId{};
    IdType toType{};
    edcom::util::Optional<uint16_t> toId{};
    uint8_t messageId{};
    uint8_t messageType{};
    uint16_t bodySize{};
};

struct Message {
    Header header{};
    uint8_t* data{};
    size_t dataLength{};
};

void recievePacket(int packetSize);

void sendMessage(const Message& message);

edcom::util::Optional<struct Header> getHeader(LoRaClass& lora,
                                               int& packetSize);

}  // namespace data

extern queue::Queue<data::Message> IncomingMessages;

}  // namespace edcom

#endif
