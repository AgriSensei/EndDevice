#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>
#include <stddef.h>
#include <stdio.h>

#define DEVICE_ID 2

template <typename T>
struct Optional {
    T value;
    bool hasVal;

    Optional() : value{}, hasVal{false} {}
    Optional(T val) : value{val}, hasVal{true} {}

    explicit operator bool() const { return hasVal; }

    T& operator*() { return value; }

    const T& operator*() const { return value; }

    T* operator->() { return &value; }

    const T* operator->() const { return &value; }
};

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
                   int remainingPacketSize) {}

void forwardToBridge(uint16_t sender, uint16_t destination,
                     int remainingPacketSize) {}

void handleFailure(FailureType type) {}

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

Optional<uint16_t> getDeviceId(int& packetSize) {
    int value = LoRa.read();
    packetSize--;

    if (value == -1 || packetSize == 0) {
        return {};
    }

    uint16_t data = (value & 0xFF);

    value = LoRa.read();
    packetSize--;

    if (value == -1) {
        return {};
    }

    data |= ((value & 0xFF) << 8);

    return data;
}

bool readRestOfMessage() {
    while (LoRa.available()) {
        LoRa.read();
    }
}

void loop() {
    int packetSize = LoRa.parsePacket();
    if (!packetSize) {
        return;
    }

    // Else, there are packets available

    Optional<uint16_t> sender = getDeviceId(packetSize);
    if (!sender || (*sender - 2) == DEVICE_ID) {
        readRestOfMessage();
        return;
    }

    Optional<uint16_t> destination = getDeviceId(packetSize);
    if (!destination) {
        readRestOfMessage();
        return;
    }
    if ((*destination - 2) == DEVICE_ID) {
        handleMessage(*sender, *destination, packetSize);
        return;
    }
    if (connectedToBridge && *destination == 0) {
        forwardToBridge(*sender, *destination, packetSize);
        return;
    }

    int messageIdPre = LoRa.read();
    packetSize--;
    if (messageIdPre == -1) {
        readRestOfMessage();
        return;
    }
    uint8_t messageId = (messageIdPre && 0xFF);

    // Check if we have seen this message before
    for (size_t i = 0; i < SEEN_DEVICES_SIZE; i++) {
        if (SEEN_DEVICES[i].deviceId == 0) {
            break;
        }

        if (SEEN_DEVICES[i].deviceId != *sender) {
            continue;
        }

        for (size_t j = 0; j < 10; j++) {
            if (SEEN_DEVICES[i].messages[j].messageId == messageId) {
                return;
            }
        }

        SEEN_DEVICES[i].messages[SEEN_DEVICES[i].mostRecentMessage].messageId =
            messageId;
    }

    // Else, we just retransmit it
    LoRa.beginPacket();
    LoRa.write(*sender);
    LoRa.write(*destination);
    while (packetSize) {
        int readByte = LoRa.read();
        if (readByte == -1) {
            break;
        }
        LoRa.write(readByte);
    }
    LoRa.endPacket();
}
