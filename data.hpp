#ifndef _DATA_HPP
#define _DATA_HPP

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "logging.hpp"
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

void printHeader(HardwareSerial& serial, struct Header& header);

struct __attribute__((packed)) Packet {
    Header header{};
    uint8_t* data{};
    size_t dataLength{};

    ~Packet() { free(data); }
};

void printPacket(HardwareSerial& serial, struct Packet& packet);

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

#ifndef DO_LOGGING
template <typename T>
size_t writeHeader(T& writeOut, const Header& header) {
    size_t bytesWritten{};
    bytesWritten += edcom::util::write2Bytes(
        writeOut, !header.fromId ? static_cast<uint16_t>(header.fromType)
                                 : *header.fromId + 2);

    bytesWritten += edcom::util::write2Bytes(
        writeOut,
        !header.toId ? static_cast<uint16_t>(header.toType) : *header.toId + 2);

    bytesWritten += writeOut.write(header.messageId);
    bytesWritten += writeOut.write(header.messageType);
    bytesWritten += edcom::util::write2Bytes(writeOut, header.bodySize);
    return bytesWritten;
}
#else
template <typename T>
size_t writeHeader(T& writeOut, const Header& header) {
    size_t bytesWritten{};
    bytesWritten += edcom::util::write2Bytes(
        writeOut, !header.fromId ? static_cast<uint16_t>(header.fromType)
                                 : *header.fromId + 2);

    bytesWritten += edcom::util::write2Bytes(
        writeOut,
        !header.toId ? static_cast<uint16_t>(header.toType) : *header.toId + 2);

    bytesWritten += writeOut.print(header.messageId);
    writeOut.print("|");
    bytesWritten += writeOut.print(header.messageType);
    writeOut.print("|");
    bytesWritten += edcom::util::write2Bytes(writeOut, header.bodySize);
    return bytesWritten;
}
#endif

template <typename T>
size_t writeData(T& writeOut, uint8_t* data, size_t size) {
    return writeOut.write(data, size);
}

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
