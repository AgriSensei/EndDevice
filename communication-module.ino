#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>
#include <stddef.h>
#include <stdio.h>

#include "data.hpp"
#include "util.hpp"

#define DEVICE_ID 2

struct MessageRecord {
    uint8_t messageId{};
    uint8_t messagesSeen{};
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
            break;
        }

        if (*SEEN_DEVICES[i].deviceId != *header.fromId) {
            continue;
        }

        deviceSeen = true;

        for (size_t j = 0; j < 10; j++) {
            if (SEEN_DEVICES[i].messages[j] &&
                SEEN_DEVICES[i].messages[j]->messageId == header.messageId) {
                Serial.println("Device + Message ID has been seen");
                return false;
            }
        }

        SEEN_DEVICES[i].messages[SEEN_DEVICES[i].nextMessageId]->messageId =
            header.messageId;

        SEEN_DEVICES[i].nextMessageId++;
        SEEN_DEVICES[i].nextMessageId %= 10;
    }

    if (!deviceSeen) {
        Serial.println("Device has not been seen before");
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
}

void loop() {
    Serial.println("trace|loop| > Recieving Packet");
    auto packetRes = edcom::data::recievePacket(LoRa, LoRa.parsePacket(),
                                                &shouldReadBodyCallback);
    Serial.println("trace|loop| < Recieving Packet");

    if (!packetRes) {
        Serial.print("warning|loop|Error when reciving packet: ");
        Serial.println(static_cast<int>(+packetRes));
        return;
    }

    auto whatToDo =
        edcom::data::whatToDoWithPacket(DEVICE_ID, Serial, *packetRes);

    if (whatToDo & edcom::data::WhatToDoWithPacket::Handle) {
        handlePacket(*packetRes);
        // TODO(EliSauder): Impl Handle
    }
    if (whatToDo & edcom::data::WhatToDoWithPacket::ForwardToBridge) {
        forwardToBridge(Serial, *packetRes);
        // TODO(EliSauder): Impl forward
    }
    if (whatToDo & edcom::data::WhatToDoWithPacket::Retransmit) {
        Serial.println("Retransmitting");
        edcom::data::sendMessage(LoRa, *packetRes);
    }
}
