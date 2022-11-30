#ifndef __ENUM_FORMAT_H__
#define __ENUM_FORMAT_H__

#include <fmt/format.h>

#include "enums.h"
#include "parse_string.h"

template<enums::enum_with_names E>
struct fmt::formatter<E> {
    template<typename Context>
    constexpr auto parse(Context &ctx) {
        return ctx.begin();
    }

    template<typename Context>
    auto format(E value, Context &ctx) {
        return fmt::format_to(ctx.out(), "{}", ::enums::to_string(value));
    }
};

template<enums::enum_with_names E> struct string_parser<E> {
    std::optional<E> operator()(std::string_view str) {
        return enums::from_string<E>(str);
    }
};

#endif