#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>
#include <stddef.h>
#include <stdio.h>

#include "data.hpp"
#include "util.hpp"

#define DEVICE_ID 0

struct MessageRecord {
    uint8_t messageId{};
    MessageRecord(uint8_t messageId) : messageId{messageId} {}
    MessageRecord() = default;
};

struct DeviceRecord {
    edcom::util::Optional<uint16_t> deviceId{};
    uint8_t nextMessageId{};
    edcom::util::Optional<MessageRecord> messages[10]{};
};

struct DeviceRecord* SEEN_DEVICES;
size_t SEEN_DEVICES_SIZE;
bool connectedToBridge = false;

enum class FailureType { LoRaInit = 1 };

void handleFailure(FailureType type) { Serial.println("handling failure"); }

void setup() {
    Serial.begin(9600);
    while (!Serial)
        ;

    if (!LoRa.begin(915E6)) {
        handleFailure(FailureType::LoRaInit);
        while (1)
            ;
    }

    SEEN_DEVICES_SIZE = 20;
    SEEN_DEVICES = (struct DeviceRecord*)malloc(sizeof(struct DeviceRecord) *
                                                SEEN_DEVICES_SIZE);
    memset(SEEN_DEVICES, 0, sizeof(struct DeviceRecord) * SEEN_DEVICES_SIZE);
}

bool shouldReadBodyCallback(struct edcom::data::Header& header) {
    bool deviceSeen = false;
    size_t i = 0;
    for (; i < SEEN_DEVICES_SIZE; i++) {
        if (!SEEN_DEVICES[i].deviceId) {
            Serial.println("info|shouldReadBodyCallback|Blank device");
            break;
        }

        if (*SEEN_DEVICES[i].deviceId != *header.fromId) {
            Serial.println("info|shouldReadBodyCallback|Device not seen");
            continue;
        }

        deviceSeen = true;
        Serial.println("info|shouldReadBodyCallback|Device has been seen");

        Serial.print("info|shouldReadBodyCallback|Current message ID: ");
        Serial.println(header.messageId);

        for (size_t j = 0; j < 10; j++) {
            if (SEEN_DEVICES[i].messages[j]) {
                Serial.print("trace|shouldReadBodyCallback|Comparing with ");
                Serial.println(SEEN_DEVICES[i].messages[j]->messageId);
                if (SEEN_DEVICES[i].messages[j]->messageId ==
                    header.messageId) {
                    Serial.println(
                        "info|shouldReadBodyCallback|Device + Message ID has "
                        "been "
                        "seen");
                    return false;
                }
            }
        }

        SEEN_DEVICES[i].messages[SEEN_DEVICES[i].nextMessageId] = {
            header.messageId};

        SEEN_DEVICES[i].nextMessageId++;
        SEEN_DEVICES[i].nextMessageId %= 10;
        break;
    }

    if (!deviceSeen) {
        Serial.println(
            "info|shouldReadBodyCallback|Device has not been seen before");
        if (i == SEEN_DEVICES_SIZE) {
            SEEN_DEVICES_SIZE += 20;
            SEEN_DEVICES = (struct DeviceRecord*)realloc(
                SEEN_DEVICES, SEEN_DEVICES_SIZE * sizeof(struct DeviceRecord));
            memset(SEEN_DEVICES + i, 0, 20 * sizeof(struct DeviceRecord));
        }
        SEEN_DEVICES[i].deviceId = *header.fromId;
    }

    return true;
}

void handlePacket(struct edcom::data::Packet& packet) {
    Serial.println("trace|handlePacket|Handling packet");
}

void forwardToBridge(HardwareSerial& serial,
                     struct edcom::data::Packet& packet) {
    Serial.println("trace|forwardToBridge|Forwarding to bridge");
    edcom::data::writeHeader(serial, packet.header);
    edcom::data::writeData(serial, packet.data, packet.dataLength);
}

void loop() {
    auto packetRes = edcom::data::recievePacket(LoRa, LoRa.parsePacket(),
                                                &shouldReadBodyCallback);

    if (!packetRes && packetRes.has_val()) {
        Serial.println("warning|loop|Recieved packet, but error reading");
        Serial.print("debug|loop|Packet Info: ");
        edcom::data::printPacket(Serial, *packetRes);
        Serial.println();
        return;
    }
    using namespace edcom::data;
    if (!packetRes) {
        switch (+packetRes) {
            case RecievePacketErrors::ExpectedPacketSizeDoesntMatchActual: {
                Serial.print(
                    "error|loop|Expected packet size doesn't match actual");
                break;
            }
            case RecievePacketErrors::NoDataAfterHeader: {
                Serial.print("error|loop|No data after header");
                break;
            }
            case RecievePacketErrors::CallbackPreventedReadingBody: {
                Serial.print("error|loop|callback prevented reading body");
                break;
            }
            case RecievePacketErrors::UnableToReadBody: {
                Serial.println("error|loop|unable to read body");
                break;
            }
            case RecievePacketErrors::UnableToReadHeader: {
                Serial.println("error|loop|unable to read header");
                break;
            }
        }
        return;
    }
    Serial.println("info|loog|Recieved packet");
    Serial.print("debug|loop|Packet Info: ");
    edcom::data::printPacket(Serial, *packetRes);
    Serial.println();

    auto whatToDo =
        edcom::data::whatToDoWithPacket(DEVICE_ID, Serial, *packetRes);

    if (whatToDo & edcom::data::WhatToDoWithPacket::Handle) {
        Serial.println("trace|loop|handling");
        handlePacket(*packetRes);
        // TODO(EliSauder): Impl Handle
    }
    if (whatToDo & edcom::data::WhatToDoWithPacket::ForwardToBridge) {
        Serial.println("trace|loop|forwarding");
        forwardToBridge(Serial, *packetRes);
        // TODO(EliSauder): Impl forward
    }
    if (whatToDo & edcom::data::WhatToDoWithPacket::Retransmit) {
        Serial.println("trace|loop|retransmitting");
        edcom::data::sendMessage(LoRa, *packetRes);
    }

    Serial.println("trace|loop|done");
}
