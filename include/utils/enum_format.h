#ifndef __ENUM_FORMAT_H__
#define __ENUM_FORMAT_H__

#include <fmt/format.h>

#include "enums.h"
#include "parse_string.h"

template<enums::enum_with_names E>
struct fmt::formatter<E> : fmt::formatter<std::string_view> {
    template<typename Context>
    auto format(E value, Context &ctx) {
        return fmt::formatter<std::string_view>::format(::enums::to_string(value), ctx);
    }
};

template<enums::enum_with_names E> struct string_parser<E> {
    std::optional<E> operator()(std::string_view str) {
        return enums::from_string<E>(str);
    }
};

#endif