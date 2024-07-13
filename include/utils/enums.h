#ifndef __ENUMS_H__
#define __ENUMS_H__

#include <magic_enum/magic_enum.hpp>
#include <stdexcept>

#include "json_serial.h"

namespace enums {

    template<typename T> concept enumeral = magic_enum::is_scoped_enum_v<T>;

    template<enumeral T> constexpr std::string_view enum_name_v = magic_enum::enum_type_name<T>();

    template<enumeral T> constexpr auto enum_values_v = magic_enum::enum_values<T>();

    template<enumeral T> constexpr size_t num_members_v = magic_enum::enum_count<T>();

    template<enumeral T>
    constexpr std::string_view to_string(T value) {
        return magic_enum::enum_name(value);
    }

    template<enumeral T>
    constexpr std::optional<T> from_string(std::string_view str) {
        return magic_enum::enum_cast<T>(str);
    }

    template<enumeral T>
    constexpr size_t indexof(T value) {
        if (auto result = magic_enum::enum_index(value)) {
            return *result;
        }
        throw std::out_of_range("invalid enum index");
    }

    template<enumeral T>
    constexpr T index_to(size_t index) {
        return magic_enum::enum_value<T>(index);
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