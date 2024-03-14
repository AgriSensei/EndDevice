#include "data.hpp"

#include <utility>

#include "logging.hpp"

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
    auto messageType = edu::getByte(lora, packetSize);
    if (!messageType) {
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
    header.messageType = *messageType;
    header.bodySize = *bodySize;
    return header;
}

enum class ReadBodyErrors {
    BufferTooSmall = 0,
    NothingToRead = 1,
};

util::Result<size_t, ReadBodyErrors> readBody(LoRaClass& lora, int& packetSize,
                                              uint8_t* buf, size_t bufLen) {
    if (packetSize <= 0) {
        return ReadBodyErrors::NothingToRead;
    }

    size_t numRead = lora.readBytes(buf, bufLen);
    packetSize -= static_cast<int>(numRead);

    if (packetSize > 0) {
        return {numRead, ReadBodyErrors::BufferTooSmall};
    }

    return numRead;
}

void readBodyAndDiscard(LoRaClass& lora) {
    while (lora.read() != -1) {
    }
}

util::Result<Packet, RecievePacketErrors> recievePacket(
    LoRaClass& lora, int packetSize, bool (*shouldReadBody)(struct Header&)) {
    if (packetSize == 0) {
        return RecievePacketErrors::NoPacketToRead;
    }

    // Read header
    auto header = getHeader(lora, packetSize);
    if (!header) {
        LOGLN("error|recievePacket|Unable to read header");
        return RecievePacketErrors::UnableToReadHeader;
    }
    if (packetSize <= 0) {
        LOGLN("error|recievePacket|No data after header");
        return RecievePacketErrors::NoDataAfterHeader;
    }

    // Handle duplicates or other things based on callback
    if (shouldReadBody != nullptr && !shouldReadBody(*header)) {
        readBodyAndDiscard(lora);
        LOGLN("error|recievePacket|Callback prevented reading body");
        return RecievePacketErrors::CallbackPreventedReadingBody;
    }

    // Read body
    util::Optional<RecievePacketErrors> bodySizeError;
    if (packetSize != header->bodySize) {
        bodySizeError =
            RecievePacketErrors::ExpectedPacketSizeDoesntMatchActual;
    }

    int toReadBodySize =
        util::min(packetSize, static_cast<int>(header->bodySize));
    auto buffer =
        static_cast<uint8_t*>(malloc(sizeof(uint8_t) * toReadBodySize));

    LOGLN("trace|recievePacket|reading body");
    auto readBodyRes = readBody(lora, toReadBodySize, buffer, toReadBodySize);
    LOGLN("trace|recievePacket|finished reading body");

    // Return final result
    if (!readBodyRes && readBodyRes.has_val()) {
        LOGLN(
            "warning|recievePacket|Error occured while reading packet, but "
            "still got data");
        Packet packet;
        packet.header = std::move(*header);
        packet.data = buffer;
        packet.dataLength = *readBodyRes;
        return {packet, RecievePacketErrors::UnableToReadBody};
    }
    if (!readBodyRes && !readBodyRes.has_val()) {
        LOGLN("error|recievePacket|Error while reading body, unable to read");
        free(buffer);
        return RecievePacketErrors::UnableToReadBody;
    }

    LOGLN("trace|recievePacket|Returning packet info");
    Packet packet;
    packet.header = std::move(*header);
    packet.data = buffer;
    packet.dataLength = *readBodyRes;
    if (bodySizeError) {
        return {packet, *bodySizeError};
    }
    return packet;
}

void recievePacket(int packetSize) {
    auto res = recievePacket(LoRa, packetSize);
    if (res) {
        IncomingMessages.enqueue(*res);
    }
}

const static size_t PACKET_NOT_READY_TRY_AGAIN_DELAY = 10;

bool sendMessage(LoRaClass& lora, const Packet& message) {
    while (!lora.beginPacket()) {
        delay(PACKET_NOT_READY_TRY_AGAIN_DELAY);
    }

    LOG("Retransmitting with fromId: ");
    LOGLN(*message.header.fromId);
    LOG("Retransmitting with toId: ");
    LOGLN(*message.header.toId);
    writeHeader(lora, message.header);
    writeData(lora, message.data, message.dataLength);

    return lora.endPacket();
}

WhatToDoWithPacket whatToDoWithPacket(uint8_t currentDeviceId,
                                      HardwareSerial& serial,
                                      struct Packet& packet) {
    if (packet.header.toType == IdType::Bridge) {
        if (serial) {
            return WhatToDoWithPacket::ForwardToBridge;
        }
        return WhatToDoWithPacket::Retransmit;
    }

    if (packet.header.toType == IdType::AnyEndDevice) {
        return static_cast<WhatToDoWithPacket>(WhatToDoWithPacket::Retransmit |
                                               WhatToDoWithPacket::Handle);
    }

    if (*packet.header.toId == currentDeviceId) {
        return WhatToDoWithPacket::Handle;
    }

    if (*packet.header.fromId == currentDeviceId) {
        return WhatToDoWithPacket::Nothing;
    }

    return WhatToDoWithPacket::Retransmit;
}
#ifdef DO_LOGGING
void printPacket(HardwareSerial& serial, struct Packet& packet) {
    serial.print("{\"header\":");
    printHeader(serial, packet.header);
    serial.print(",");
    serial.print("\"data\":\"");
    serial.write(packet.data, packet.dataLength);
    serial.print("\"}");
}
#else
void printPacket(HardwareSerial& serial, struct Packet& packet) {}
#endif

#ifdef DO_LOGGING
void printHeader(HardwareSerial& serial, struct Header& header) {
    serial.print("{");
    serial.print("\"fromType\":\"");
    switch (header.fromType) {
        case IdType::AnyEndDevice: {
            serial.print("AnyEndDevice");
            break;
        }
        case IdType::SpecificEndDevice: {
            serial.print("SpecificEndDevice");
            break;
        }
        case IdType::Bridge: {
            serial.print("Bridge");
            break;
        }
    }
    serial.print("\",");
    if (header.fromId) {
        serial.print("\"fromId\":\"");
        serial.print(*header.fromId);
        serial.print("\",");
    }
    serial.print("\"toType\":\"");
    switch (header.toType) {
        case IdType::AnyEndDevice: {
            serial.print("AnyEndDevice");
            break;
        }
        case IdType::SpecificEndDevice: {
            serial.print("SpecificEndDevice");
            break;
        }
        case IdType::Bridge: {
            serial.print("Bridge");
            break;
        }
    }
    serial.print("\",");
    if (header.toId) {
        serial.print("\"toId\":\"");
        serial.print(*header.toId);
        serial.print("\",");
    }
    serial.print("\"messageId\":\"");
    serial.print(header.messageId);
    serial.print("\",");
    serial.print("\"messageType\":\"");
    serial.print(header.messageType);
    serial.print("\",");
    serial.print("\"bodySize\":\"");
    serial.print(header.bodySize);
    serial.print("\"}");
}
#else
void printHeader(HardwareSerial& serial, struct Header& header) {}
#endif

}  // namespace data
}  // namespace edcom
