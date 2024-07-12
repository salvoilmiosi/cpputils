#ifndef __ENUM_FORMAT_H__
#define __ENUM_FORMAT_H__

#include <fmt/format.h>

#include "enum_bitset.h"
#include "parse_string.h"

template<enums::enumeral E>
struct fmt::formatter<E> : fmt::formatter<std::string_view> {
    template<typename Context>
    auto format(E value, Context &ctx) {
        return fmt::formatter<std::string_view>::format(::enums::to_string(value), ctx);
    }
};

template<enums::enumeral E>
struct fmt::formatter<enums::bitset<E>> : fmt::formatter<std::string_view> {
    static constexpr std::string bitset_to_string(::enums::bitset<E> value) {
        std::string ret;
        for (E v : ::enums::enum_values_v<E>) {
            if (value.check(v)) {
                if (!ret.empty()) {
                    ret += ' ';
                }
                ret.append(::enums::to_string(v));
            }
        }
        return ret;
    }

    template<typename Context>
    auto format(::enums::bitset<E> value, Context &ctx) {
        return fmt::formatter<std::string_view>::format(bitset_to_string(value), ctx);
    }
};

template<enums::enumeral E>
struct string_parser<enums::bitset<E>> {
    constexpr std::optional<enums::bitset<E>> operator()(std::string_view str) {
        constexpr std::string_view whitespace = " \t";
        enums::bitset<E> result;
        while (true) {
            size_t pos = str.find_first_not_of(whitespace);
            if (pos == std::string_view::npos) break;
            str = str.substr(pos);
            pos = str.find_first_of(whitespace);
            if (auto value = enums::from_string<E>(str.substr(0, pos))) {
                result.add(*value);
            } else {
                return std::nullopt;
            }
            if (pos == std::string_view::npos) break;
            str = str.substr(pos);
        }
        return result;
    }
};

#endif