#ifndef __ENUMS_H__
#define __ENUMS_H__

#include <reflect>
#include <stdexcept>

#include "json_serial.h"

namespace enums {

    template<typename T> concept enumeral = std::is_enum_v<T>;

    template<enumeral T> constexpr std::string_view enum_type_name() {
        return reflect::type_name<T>();
    }

    template<enumeral T, typename ISeq> struct build_enum_values;
    template<enumeral T, size_t ... Is> struct build_enum_values<T, std::index_sequence<Is ...>> {
        static constexpr std::array value { static_cast<T>(reflect::enumerators<T>[Is].first) ... };
    };

    template<enumeral T> constexpr const auto &enum_values() {
        return build_enum_values<T, std::make_index_sequence<reflect::enumerators<T>.size()>>::value;
    }

    template<enumeral T>
    constexpr bool is_linear_enum() {
        size_t i=0;
        for (T value : enum_values<T>()) {
            if (static_cast<size_t>(value) != i) {
                return false;
            }
            ++i;
        }
        return true;
    }

    template<enumeral T>
    constexpr size_t indexof(T value) {
        constexpr const auto &values = enum_values<T>();
        if constexpr (is_linear_enum<T>()) {
            size_t result = static_cast<size_t>(value);
            if (result >= 0 && result <= static_cast<size_t>(values.back())) {
                return result;
            }
        } else {
            for (size_t i=0; i<values.size(); ++i) {
                if (values[i] == value) {
                    return i;
                }
            }
        }
        throw std::out_of_range("invalid enum index");
    }

    template<enumeral T>
    constexpr std::string_view to_string(T input) {
        return reflect::enumerators<T>[indexof(input)].second;
    }

    template<enumeral T>
    constexpr std::optional<T> from_string(std::string_view str) {
        for (const auto &[value, name] : reflect::enumerators<T>) {
            if (name == str) {
                return static_cast<T>(value);
            }
        }
        return std::nullopt;
    }

    template<enumeral auto ... Values> struct enum_sequence {
        static constexpr size_t size = sizeof...(Values);
    };
    
}

namespace json {

    template<enums::enumeral T, typename Context>
    struct deserializer<T, Context> {
        T operator()(const json &value) const {
            if (value.is_string()) {
                auto str = value.get<std::string>();
                if (auto ret = enums::from_string<T>(str)) {
                    return *ret;
                } else {
                    throw std::runtime_error(fmt::format("Invalid {}: {}", enums::enum_type_name<T>(), str));
                }
            } else {
                throw std::runtime_error("Value is not a string");
            }
        }
    };

    template<enums::enumeral T, typename Context>
    struct serializer<T, Context> {
        json operator()(const T &value) const {
            return std::string(enums::to_string(value));
        }
    };

}

#endif