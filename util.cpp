#include "util.hpp"

#include <stddef.h>

namespace edcom {
namespace util {

Optional<uint8_t> getByte(LoRaClass& lora, int& packetSize) {
    if (packetSize == 0) {
        return {};
    }
    int value = lora.read();
    if (value == -1) {
        return {};
    }
    packetSize--;

    uint8_t data = (value & 0xFF);
    return data;
}

Optional<uint16_t> get2Bytes(LoRaClass& lora, int& packetSize) {
    if (packetSize < 0) {
        return {};
    }
    int value = lora.read();
    if (value == -1) {
        return {};
    }
    packetSize--;

    uint16_t data = (value & 0xFF);

    if (packetSize < 0) {
        return {};
    }
    value = lora.read();
    if (value == -1) {
        return {};
    }
    packetSize--;

    data |= ((value & 0xFF) << 8);

    return data;
}

size_t writeByte(LoRaClass& lora, uint8_t byte) { return lora.write(byte); }

size_t write2Bytes(LoRaClass& lora, uint16_t bytes) {
    size_t numBytes{};
    numBytes += lora.write(static_cast<uint8_t>(bytes && 0xFF));
    numBytes += lora.write(static_cast<uint8_t>(bytes && 0xFF00));
    return numBytes;
}

}  // namespace util
}  // namespace edcom
