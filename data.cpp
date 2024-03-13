#include "data.hpp"

namespace edcom {
queue::Queue<data::Message> IncomingMessages;

namespace data {
namespace edu = edcom::util;

edu::Optional<edu::Pair<edu::Optional<uint16_t>, IdType>> getIdInfo(
    edu::Optional<uint16_t> id) {
    if (!id) {
        return edu::Optional<edu::Pair<edu::Optional<uint16_t>, IdType>>{};
    } else if (*id == 0) {
        return edu::Pair<edu::Optional<uint16_t>, IdType>(
            edu::Optional<uint16_t>{}, IdType::Bridge);
    } else if (*id == 1) {
        return edu::Pair<edu::Optional<uint16_t>, IdType>{
            edu::Optional<uint16_t>{}, IdType::AnyEndDevice};
    }

    return edu::Pair<edu::Optional<uint16_t>, IdType>{
        edu::Optional<uint16_t>{uint16_t(*id - 2)}, IdType::SpecificEndDevice};
}

edu::Optional<struct Header> getHeader(LoRaClass& lora, int& packetSize) {
    auto fromIdInfo = getIdInfo(edu::get2Bytes(lora, packetSize));
    if (!fromIdInfo) {
        return {};
    }
    auto toIdInfo = getIdInfo(edu::get2Bytes(lora, packetSize));
    if (!toIdInfo) {
        return {};
    }
    auto messageId = edu::getByte(lora, packetSize);
    if (!messageId) {
        return {};
    }
    auto bodySize = edu::get2Bytes(lora, packetSize);
    if (!bodySize) {
        return {};
    }

    struct Header header;
    header.fromType = fromIdInfo->second;
    header.fromId = fromIdInfo->first;
    header.toType = toIdInfo->second;
    header.toId = toIdInfo->first;
    header.messageId = *messageId;
    header.bodySize = *bodySize;
    return header;
}

void recievePacket(int packetSize) {}

void sendMessage(Message message) {}

}  // namespace data
}  // namespace edcom
