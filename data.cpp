#include "data.hpp"

#include <utility>

namespace edcom {
queue::Queue<data::Packet> IncomingMessages;

namespace data {
namespace edu = edcom::util;

edu::Optional<edu::Pair<edu::Optional<uint16_t>, IdType>> getIdInfo(
    edu::Optional<uint16_t> identifier) {
    if (!identifier) {
        return {};
    }
    if (*identifier == 0) {
        return {{{}, IdType::Bridge}};
    }
    if (*identifier == 1) {
        return {{{}, IdType::AnyEndDevice}};
    }

    return {
        {{static_cast<uint16_t>(*identifier - 2)}, IdType::SpecificEndDevice}};
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

enum class ReadBodyErrors {
    BufferTooSmall = 0,
    NothingToRead = 0,
};

util::Result<size_t, ReadBodyErrors> readBody(LoRaClass& lora, int& packetSize,
                                              uint8_t* buf, size_t bufLen) {
    if (packetSize <= 0) {
        return ReadBodyErrors::NothingToRead;
    }

    size_t writeIndex{};
    while (packetSize-- > 0 && bufLen-- > 0) {
        *(buf + writeIndex++) = lora.read();
    }

    if (packetSize > 0) {
        return {writeIndex, ReadBodyErrors::BufferTooSmall};
    }

    return writeIndex;
}

void recievePacket(int packetSize) {
    auto res = recievePacket(LoRa, packetSize);
    if (res) {
        IncomingMessages.enqueue(*res);
    }
}

util::Result<Packet, RecievePacketErrors> recievePacket(LoRaClass& lora,
                                                        int packetSize) {
    if (packetSize == 0) {
        return RecievePacketErrors::NoPacketToRead;
    }

    auto header = getHeader(lora, packetSize);
    if (!header) {
        return RecievePacketErrors::UnableToReadHeader;
    }

    if (packetSize <= 0) {
        return RecievePacketErrors::NoDataAfterHeader;
    }

    util::Optional<RecievePacketErrors> bodySizeError;

    if (packetSize != header->bodySize) {
        bodySizeError =
            RecievePacketErrors::ExpectedPacketSizeDoesntMatchActual;
    }

    int toReadBodySize =
        util::min(packetSize, static_cast<int>(header->bodySize));

    auto buffer =
        static_cast<uint8_t*>(malloc(sizeof(uint8_t) * toReadBodySize));

    auto readBodyRes = readBody(lora, toReadBodySize, buffer, toReadBodySize);

    if (!readBodyRes && readBodyRes.has_val()) {
        Packet packet;
        packet.header = std::move(*header);
        packet.data = buffer;
        packet.dataLength = *readBodyRes;
        return {packet, RecievePacketErrors::UnableToReadBody};
    }
    if (!readBodyRes && !readBodyRes.has_val()) {
        free(buffer);
        return RecievePacketErrors::UnableToReadBody;
    }
    Packet packet;
    packet.header = std::move(*header);
    packet.data = buffer;
    packet.dataLength = *readBodyRes;
    if (bodySizeError) {
        return {packet, *bodySizeError};
    }
    return packet;
}

size_t writeHeader(LoRaClass& lora, const Header& header) {
    size_t bytesWritten{};
    bytesWritten += lora.write(*header.fromId);

    bytesWritten += edcom::util::write2Bytes(
        lora,
        header.toId ? static_cast<uint16_t>(header.toType) : *header.toId);

    bytesWritten += lora.write(header.messageId);
    bytesWritten += lora.write(header.messageType);
    bytesWritten += edcom::util::write2Bytes(lora, header.bodySize);
    return bytesWritten;
}

size_t writeData(LoRaClass& lora, uint8_t* data, size_t size) {
    return lora.write(data, size);
}

const static size_t PACKET_NOT_READY_TRY_AGAIN_DELAY = 10;

bool sendMessage(LoRaClass& lora, const Packet& message) {
    while (!lora.beginPacket()) {
        delay(PACKET_NOT_READY_TRY_AGAIN_DELAY);
    }

    writeHeader(lora, message.header);
    writeData(lora, message.data, message.dataLength);

    return lora.endPacket();
}

}  // namespace data
}  // namespace edcom
