#include "util.hpp"

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

}  // namespace util
}  // namespace edcom
