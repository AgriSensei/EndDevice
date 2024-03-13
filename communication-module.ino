#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>
#include <stddef.h>
#include <stdio.h>

#include "util.hpp"
#include "data.hpp"

#define DEVICE_ID 2

struct MessageRecord {
    uint8_t messageId;
    uint8_t messagesSeen;
};
struct DeviceRecord {
    uint16_t deviceId;
    uint8_t mostRecentMessage;
    MessageRecord messages[10];
};

struct DeviceRecord* SEEN_DEVICES;
size_t SEEN_DEVICES_SIZE;
bool connectedToBridge = false;

enum class FailureType { LoRaInit = 1 };

void handleMessage(uint16_t sender, uint16_t destination,
                   int remainingPacketSize) {
    Serial.println("handling message");
}

void forwardToBridge(uint16_t sender, uint16_t destination,
                     int remainingPacketSize) {
    Serial.println("forwarding message to bridge");
}

void handleFailure(FailureType type) { Serial.println("handling failure"); }


bool readRestOfMessage() {
    Serial.println("Reading rest of message");
    while (LoRa.read() != -1)
        ;
}

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

void loop() {
    int packetSize = LoRa.parsePacket();
    if (!packetSize) {
        return;
    }
    Serial.println("Message recieved");

    uint8_t* messageContent = nullptr;
    size_t messageContentSize = 0;
    if (packetSize - 4 > 0) {
        messageContentSize = packetSize - 4;
        messageContent = (uint8_t*)malloc(sizeof(uint8_t) * packetSize - 4);
    }

    // Else, there are packets available

    auto headerOpt = edcom::data::getHeader(LoRa, packetSize);
    if (!headerOpt ||
        (headerOpt->fromId && *(headerOpt->fromId) - 2 == DEVICE_ID)) {
        readRestOfMessage();
        return;
    }
    edcom::data::Header header = *headerOpt;

    if (header.toId && *(header.toId) == DEVICE_ID) {
        handleMessage(*header.fromId, *header.toId, packetSize);
        return;
    }
    if (connectedToBridge && header.toType == edcom::data::IdType::Bridge) {
        forwardToBridge(*header.fromId, static_cast<uint16_t>(edcom::data::IdType::Bridge),
                        packetSize);
        return;
    }

    // Check if we have seen this message before
    bool deviceSeen = false;
    size_t i = 0;
    for (; i < SEEN_DEVICES_SIZE; i++) {
        if (SEEN_DEVICES[i].deviceId == 0) {
            break;
        }

        if (SEEN_DEVICES[i].deviceId != *header.fromId) {
            continue;
        }

        deviceSeen = true;

        for (size_t j = 0; j < 10; j++) {
            if (SEEN_DEVICES[i].messages[j].messageId == header.messageId) {
                Serial.println("Device + Message ID has been seen");
                return;
            }
        }

        SEEN_DEVICES[i].messages[SEEN_DEVICES[i].mostRecentMessage].messageId =
            header.messageId;
        SEEN_DEVICES[i].mostRecentMessage =
            (SEEN_DEVICES[i].mostRecentMessage + 1) % 1;
    }

    if (!deviceSeen) {
        Serial.println("Device has not been seen before");
        i++;
        if (i == SEEN_DEVICES_SIZE) {
            SEEN_DEVICES_SIZE += 20;
            SEEN_DEVICES = (struct DeviceRecord*)realloc(
                SEEN_DEVICES, SEEN_DEVICES_SIZE * sizeof(struct DeviceRecord));
            memset(SEEN_DEVICES + i, 0, 20 * sizeof(struct DeviceRecord));
        }
        SEEN_DEVICES[i].deviceId = *header.fromId;
    }

    Serial.println("Reading rest of message into buffer");
    uint8_t byteRead = LoRa.read();
    size_t index = 0;
    while (byteRead != -1 && index < messageContentSize) {
        *(messageContent + index++) = byteRead;
        byteRead = LoRa.read();
    }
    if (index < messageContentSize) {
        messageContentSize = index;
    }

    delay(100);

    // Else, we just retransmit it
    Serial.println("Retransmitting");
    while (!LoRa.beginPacket()) {
        Serial.println("Trying again");
        delay(10);
    }
    Serial.println("Retransmitting 2");
    LoRa.write(*header.fromId);
    LoRa.write(header.toId ? static_cast<uint16_t>(header.toType)
                           : *header.toId);
    // TODO: Write header
    for (size_t i = 0; i < index; i++) {
        LoRa.write(*(messageContent + i));
    }
    LoRa.endPacket();
}
