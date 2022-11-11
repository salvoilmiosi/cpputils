#ifndef __PARSE_STRING_H__
#define __PARSE_STRING_H__

#include <string>
#include <charconv>
#include <optional>

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

template<typename T>
std::optional<T> parse_string(std::string_view str) {
    return string_parser<T>{}(str);
}

#endif