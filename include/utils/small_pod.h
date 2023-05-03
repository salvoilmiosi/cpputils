#ifndef __SMALL_POD_H__
#define __SMALL_POD_H__

#include <array>
#include <string>
#include <cstring>
#include <stdexcept>

#include "json_serial.h"

template<size_t MaxSize>
class basic_small_string {
private:
    std::array<char, MaxSize> m_str{};

    constexpr void init(std::string_view str) {
        std::memcpy(m_str.data(), str.data(), str.size());
    }

public:
    constexpr basic_small_string() = default;

    constexpr basic_small_string(std::string_view str) {
        if (str.size() > MaxSize) {
            throw std::runtime_error("String is too large");
        }
        init(str);
    }

    template<size_t N>
    constexpr basic_small_string(const char (&str)[N]) {
        static_assert(N - 1 <= MaxSize, "String is too large");
        init(std::string_view(std::begin(str), std::end(str)-1));
    }

    constexpr basic_small_string(std::same_as<const char *> auto str)
        : basic_small_string(std::string_view(str)) {}

    constexpr bool empty() const {
        return m_str.front() == '\0';
    }

    constexpr size_t size() const {
        const char *ptr = m_str.data();
        while (ptr != m_str.data() + MaxSize && *ptr) {
            ++ptr;
        }
        return ptr - m_str.data();
    }

    constexpr operator std::string_view() const {
        return std::string_view{m_str.data(), m_str.data() + size()};
    }
};

using small_string = basic_small_string<32>;

namespace json {
    template<size_t MaxSize, typename Context>
    struct serializer<basic_small_string<MaxSize>, Context> {
        json operator()(const basic_small_string<MaxSize> &value) const {
            return std::string(std::string_view(value));
        }
    };

    template<size_t MaxSize, typename Context>
    struct deserializer<basic_small_string<MaxSize>, Context> {
        basic_small_string<MaxSize> operator()(const json &value) const {
            return basic_small_string<MaxSize>(value.get<std::string>());
        }
    };
}

template<typename T, size_t MaxSize = 5>
class small_vector {
private:
    std::array<T, MaxSize> m_data;
    size_t m_size = 0;

public:
    constexpr small_vector() = default;

    template<std::same_as<T> ... Ts>
    constexpr small_vector(Ts && ... values) {
        static_assert(sizeof...(Ts) <= MaxSize, "Vector is too big");
        (push_back(std::forward<Ts>(values)), ...);
    }

    constexpr void push_back(const T &value) {
        m_data[m_size++] = value;
    }

    constexpr void push_back(T &&value) {
        m_data[m_size++] = std::move(value);
    }

    constexpr void clear() {
        m_size = 0;
    }

    constexpr size_t size() const {
        return m_size;
    }

    constexpr const T *begin() const {
        return m_data.data();
    }

    constexpr const T *end() const {
        return m_data.data() + m_size;
    }
};

namespace json {
    template<typename T, size_t MaxSize, typename Context>
    struct serializer<small_vector<T, MaxSize>, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;
        
        json operator()(const small_vector<T, MaxSize> &value) const {
            auto ret = json::array();
            for (const T &obj : value) {
                ret.push_back(this->serialize_with_context(obj));
            }
            return ret;
        }
    };

    template<typename T, size_t MaxSize, typename Context>
    struct deserializer<small_vector<T, MaxSize>, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;
        
        small_vector<T, MaxSize> operator()(const json &value) const {
            if (value.size() > MaxSize) {
                throw std::runtime_error("Vector is too big");
            }
            small_vector<T, MaxSize> ret;
            for (const auto &obj : value) {
                ret.push_back(this->template deserialize_with_context<T>(obj));
            }
            return ret;
        }
    };
}

#endif