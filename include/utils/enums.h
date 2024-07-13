#ifndef __ENUMS_H__
#define __ENUMS_H__

#include <reflect>
#include <stdexcept>

#include "json_serial.h"

namespace enums {

    template<typename T> concept enumeral = std::is_enum_v<T>;

    template<enumeral T> constexpr std::string_view enum_name_v = reflect::type_name<T>();

    template<enumeral T> constexpr auto enum_values_v = []<size_t ... Is>(std::index_sequence<Is ...>){
        return std::array{ static_cast<T>(reflect::enumerators<T>[Is].first) ... };
    }(std::make_index_sequence<reflect::enumerators<T>.size()>());

    template<enumeral T> constexpr size_t num_members_v = enum_values_v<T>.size();

    template<enumeral T>
    constexpr bool is_linear_enum() {
        size_t i=0;
        for (T value : enum_values_v<T>) {
            if (static_cast<size_t>(value) != i) {
                return false;
            }
            ++i;
        }
        return true;
    }

    template<enumeral T>
    constexpr size_t indexof(T value) {
        if constexpr (is_linear_enum<T>()) {
            size_t result = static_cast<size_t>(value);
            if (result >= 0 && result <= static_cast<size_t>(enum_values_v<T>.back())) {
                return result;
            }
        } else {
            for (size_t i=0; i<enum_values_v<T>.size(); ++i) {
                if (enum_values_v<T>[i] == value) {
                    return i;
                }
            }
        }
        throw std::out_of_range("invalid enum index");
    }

    template<enumeral T>
    constexpr T index_to(size_t index) {
        if constexpr (is_linear_enum<T>()) {
            return static_cast<T>(index);
        } else {
            return enum_values_v<T>[index];
        }
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

    namespace detail {
        template<enumeral T, typename ISeq> struct make_enum_sequence{};
        template<enumeral T, size_t ... Is> struct make_enum_sequence<T, std::index_sequence<Is...>> {
            using type = enum_sequence<index_to<T>(Is)...>;
        };
    }

    template<enumeral T> using make_enum_sequence = typename detail::make_enum_sequence<T, std::make_index_sequence<num_members_v<T>>>::type;
    
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
                    throw std::runtime_error(fmt::format("Invalid {}: {}", enums::enum_name_v<T>, str));
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