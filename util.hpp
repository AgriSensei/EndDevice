#ifndef _OPTIONAL_HPP
#define _OPTIONAL_HPP

#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>
#include <stddef.h>
#include <stdint.h>

namespace edcom {
namespace util {

template <typename T>
class Optional {
   private:
    T value{};
    bool hasVal{};

   public:
    Optional() = default;
    Optional(T val) : value{val}, hasVal{true} {}

    explicit operator bool() const noexcept { return hasVal; }

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

    Pair() = default;
    Pair(F first, S second) : first{first}, second{second} {}
};

template <typename T, typename E>
class Result {
   private:
    Optional<T> value{};
    Optional<E> error{};

   public:
    Result() = default;
    Result(T value) : value{value} {}
    Result(E error) : error{error} {}
    Result(T value, E error) : value{value}, error{error} {}

    explicit operator bool() const noexcept { return !error; }

    [[nodiscard]] auto has_val() const noexcept -> bool {
        return static_cast<bool>(value);
    }

    auto operator*() -> T& { return *value; }

    auto operator*() const -> const T& { return *value; }

    auto operator+() -> E& { return *error; }

    auto operator+() const -> const E& { return *error; }

    auto operator->() -> T* { return &(*value); }

    auto operator->() const -> const T* { return &(*value); }
};

Optional<uint8_t> getByte(LoRaClass& lora, int& packetSize);

Optional<uint16_t> get2Bytes(LoRaClass& lora, int& packetSize);

size_t writeByte(LoRaClass& lora, uint8_t byte);
size_t write2Bytes(LoRaClass& lora, uint16_t byte);

template <typename T>
T min(T a, T b) {
    if (a < b) {
        return a;
    }
    return b;
}

template <typename T>
T max(T a, T b) {
    if (a > b) {
        return a;
    }
    return b;
}

}  // namespace util
}  // namespace edcom
#endif
