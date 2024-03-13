#ifndef _OPTIONAL_HPP
#define _OPTIONAL_HPP

#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>
#include <stdint.h>

namespace edcom {
namespace util {

template <typename T>
class Optional {
   private:
    T value{};
    bool hasVal{};

   public:
    Optional() : value{}, hasVal{false} {}
    Optional(T val) : value{val}, hasVal{true} {}

    explicit operator bool() const { return hasVal; }

    auto operator*() -> T& { return value; }

    auto operator*() const -> const T& { return value; }

    auto operator->() -> T* { return &value; }

    auto operator->() const -> const T* { return &value; }
};

template <typename F, typename S>
class Pair {
   public:
    F first{};
    S second{};

    Pair() {}
    Pair(F first, S second) : first{first}, second{second} {}
};

Optional<uint8_t> getByte(LoRaClass& lora, int& packetSize);

Optional<uint16_t> get2Bytes(LoRaClass& lora, int& packetSize);

}  // namespace util
}  // namespace edcom
#endif
