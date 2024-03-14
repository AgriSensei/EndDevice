#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>
#include <stddef.h>
#include <stdio.h>

#include "data.hpp"
#include "logging.hpp"
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

void setup() {
    Serial.begin(9600);

    if (!LoRa.begin(915E6)) {
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
            LOGLN("info|shouldReadBodyCallback|Blank device");
            break;
        }

        if (*SEEN_DEVICES[i].deviceId != *header.fromId) {
            LOGLN("info|shouldReadBodyCallback|Device not seen");
            continue;
        }

        deviceSeen = true;
        LOGLN("info|shouldReadBodyCallback|Device has been seen");

        LOG("info|shouldReadBodyCallback|Current message ID: ");
        LOGLN(header.messageId);

        for (size_t j = 0; j < 10; j++) {
            if (SEEN_DEVICES[i].messages[j]) {
                LOG("trace|shouldReadBodyCallback|Comparing with ");
                LOGLN(SEEN_DEVICES[i].messages[j]->messageId);
                if (SEEN_DEVICES[i].messages[j]->messageId ==
                    header.messageId) {
                    LOGLN(
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
        LOGLN("info|shouldReadBodyCallback|Device has not been seen before");
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
    LOGLN("trace|handlePacket|Handling packet");
}

void forwardToBridge(HardwareSerial& serial,
                     struct edcom::data::Packet& packet) {
    LOGLN("trace|forwardToBridge|Forwarding to bridge");
    edcom::data::writeHeader(serial, packet.header);
    edcom::data::writeData(serial, packet.data, packet.dataLength);
}

void loop() {
    auto packetRes = edcom::data::recievePacket(LoRa, LoRa.parsePacket(),
                                                &shouldReadBodyCallback);

    if (!packetRes && packetRes.has_val()) {
        LOGLN("warning|loop|Recieved packet, but error reading");
        LOG("debug|loop|Packet Info: ");
        edcom::data::printPacket(Serial, *packetRes);
        LOGLN();
        return;
    }
    using namespace edcom::data;
    if (!packetRes) {
        switch (+packetRes) {
            case RecievePacketErrors::ExpectedPacketSizeDoesntMatchActual: {
                LOG("error|loop|Expected packet size doesn't match actual");
                break;
            }
            case RecievePacketErrors::NoDataAfterHeader: {
                LOG("error|loop|No data after header");
                break;
            }
            case RecievePacketErrors::CallbackPreventedReadingBody: {
                LOG("error|loop|callback prevented reading body");
                break;
            }
            case RecievePacketErrors::UnableToReadBody: {
                LOGLN("error|loop|unable to read body");
                break;
            }
            case RecievePacketErrors::UnableToReadHeader: {
                LOGLN("error|loop|unable to read header");
                break;
            }
        }
        return;
    }
    LOGLN("info|loog|Recieved packet");
    LOG("debug|loop|Packet Info: ");
    edcom::data::printPacket(Serial, *packetRes);
    LOGLN();

    auto whatToDo =
        edcom::data::whatToDoWithPacket(DEVICE_ID, Serial, *packetRes);

    if (whatToDo & edcom::data::WhatToDoWithPacket::Handle) {
        LOGLN("trace|loop|handling");
        handlePacket(*packetRes);
        // TODO(EliSauder): Impl Handle
    }
    if (whatToDo & edcom::data::WhatToDoWithPacket::ForwardToBridge) {
        LOGLN("trace|loop|forwarding");
        forwardToBridge(Serial, *packetRes);
        // TODO(EliSauder): Impl forward
    }
    if (whatToDo & edcom::data::WhatToDoWithPacket::Retransmit) {
        LOGLN("trace|loop|retransmitting");
        edcom::data::sendMessage(LoRa, *packetRes);
    }

    LOGLN("trace|loop|done");
}
