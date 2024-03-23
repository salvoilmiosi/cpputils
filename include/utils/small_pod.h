#ifndef __SMALL_POD_H__
#define __SMALL_POD_H__

#include <array>
#include <string>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <initializer_list>
#include <bit>

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

class small_int_set_iterator {
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using value_type = int;
    using reference = int;

private:
    uint8_t value;
    uint8_t index;

public:
    constexpr small_int_set_iterator(uint8_t value, uint8_t index)
        : value{value}, index{index} {}
    
    constexpr int operator *() const {
        return index;
    }

    constexpr small_int_set_iterator &operator++() {
        uint8_t mask = (1 << (index + 1)) - 1;
        index = std::countr_zero<uint8_t>(value & ~mask);
        return *this;
    }

    constexpr small_int_set_iterator operator++(int) {
        auto copy = *this;
        ++*this;
        return copy;
    }

    constexpr small_int_set_iterator &operator--() {
        uint8_t mask = (1 << index) - 1;
        index = 7 - std::countl_zero<uint8_t>(value & mask);
        return *this;
    }

    constexpr small_int_set_iterator operator--(int) {
        auto copy = *this;
        --*this;
        return copy;
    }

    constexpr bool operator == (const small_int_set_iterator &other) const = default;
};

class small_int_set {
private:
    uint8_t m_value{};

public:
    constexpr small_int_set(std::initializer_list<int> values) {
        int prev = -1;
        for (int value : values) {
            if (value < 0 || value >= 8) {
                throw std::runtime_error("invalid small_int_set, ints must be in range 0-7");
            }
            if (value <= prev) {
                throw std::runtime_error("invalid small_int_set, values must be in ascending order");
            }
            m_value |= 1 << value;
            prev = value;
        }
    }

    constexpr auto begin() const {
        return small_int_set_iterator{m_value, static_cast<uint8_t>(std::countr_zero(m_value))};
    }

    constexpr auto end() const {
        return small_int_set_iterator{m_value, 8};
    }

    constexpr size_t size() const {
        return std::popcount(m_value);
    }

    constexpr int operator[](size_t index) const {
        return *(std::next(begin(), index));
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

    template<typename Context>
    struct serializer<small_int_set, Context> {
        json operator()(small_int_set value) const {
            auto ret = json::array();
            for (int n : value) {
                ret.push_back(n);
            }
            return ret;
        }
    };
}

#endif