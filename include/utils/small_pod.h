#ifndef __SMALL_POD_H__
#define __SMALL_POD_H__

#include <array>
#include <string>
#include <algorithm>

#include "json_serial.h"

template<size_t MaxSize>
class basic_small_string {
private:
    std::array<char, MaxSize> m_str;
    size_t m_size = 0;

public:
    constexpr basic_small_string() = default;

    template<size_t N>
    constexpr basic_small_string(const char (&str)[N])
        : m_size{N - 1}
    {
        static_assert(N - 1 <= MaxSize, "String is too large");
        std::copy(str, str + m_size, m_str.begin());
    }

    constexpr bool empty() const {
        return m_size == 0;
    }

    constexpr size_t size() const {
        return m_size;
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
}

#endif