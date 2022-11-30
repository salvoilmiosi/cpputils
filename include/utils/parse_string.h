#ifndef __PARSE_STRING_H__
#define __PARSE_STRING_H__

#include <fmt/format.h>

#include <string>
#include <charconv>
#include <optional>
#include <chrono>

template<typename T> struct string_parser;

template<std::integral T> struct string_parser<T> {
    std::optional<T> operator()(std::string_view str) {
        T value;
        if (auto [end, ec] = std::from_chars(str.data(), str.data() + str.size(), value); ec != std::errc{}) {
            return std::nullopt;
        }
        return value;
    }
};

template<> struct string_parser<bool> {
    std::optional<bool> operator()(std::string_view str) {
        if (str == "true") {
            return true;
        } else if (str == "false") {
            return false;
        } else {
            return std::nullopt;
        }
    }
};

template<typename Rep, typename Period>
struct string_parser<std::chrono::duration<Rep, Period>> {
    std::optional<std::chrono::duration<Rep, Period>> operator()(std::string_view str) {
        if (auto value = string_parser<Rep>{}(str)) {
            return std::chrono::duration<Rep, Period>{*value};
        } else {
            return std::nullopt;
        }
    }
};

template<typename Rep, typename Period>
struct fmt::formatter<std::chrono::duration<Rep, Period>> {
    template<typename Context>
    constexpr auto parse(Context &ctx) {
        return ctx.begin();
    }

    template<typename Context>
    auto format(const std::chrono::duration<Rep, Period> &value, Context &ctx) {
        return fmt::format_to(ctx.out(), "{}", value.count());
    }
};

template<typename T>
std::optional<T> parse_string(std::string_view str) {
    return string_parser<T>{}(str);
}

#endif