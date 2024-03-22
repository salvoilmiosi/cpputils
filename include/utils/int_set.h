#ifndef __UTILS_INT_SET_H__
#define __UTILS_INT_SET_H__

#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <iterator>
#include <bit>

namespace utils {
    class int_set_iterator {
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
        constexpr int_set_iterator(uint8_t value, uint8_t index)
            : value{value}, index{index} {}
        
        constexpr int operator *() const {
            return index;
        }

        constexpr int_set_iterator &operator++() {
            uint8_t mask = (1 << (index + 1)) - 1;
            index = std::countr_zero<uint8_t>(value & ~mask);
            return *this;
        }

        constexpr int_set_iterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        constexpr int_set_iterator &operator--() {
            uint8_t mask = (1 << index) - 1;
            index = 7 - std::countl_zero<uint8_t>(value & mask);
            return *this;
        }

        constexpr int_set_iterator operator--(int) {
            auto copy = *this;
            --*this;
            return copy;
        }

        constexpr bool operator == (const int_set_iterator &other) const = default;
    };

    class int_set {
    private:
        uint8_t m_value{};

    public:
        constexpr int_set(std::initializer_list<int> values) {
            int prev = -1;
            for (int value : values) {
                if (value < 0 || value >= 8) {
                    throw std::runtime_error("invalid int_set, ints must be in range 0-7");
                }
                if (value <= prev) {
                    throw std::runtime_error("invalid int_set, values must be in ascending order");
                }
                m_value |= 1 << value;
                prev = value;
            }
        }

        constexpr auto begin() const {
            return int_set_iterator{m_value, static_cast<uint8_t>(std::countr_zero(m_value))};
        }

        constexpr auto end() const {
            return int_set_iterator{m_value, 8};
        }

        constexpr size_t size() const {
            return std::popcount(m_value);
        }

        constexpr int operator[](size_t index) const {
            return *(std::next(begin(), index));
        }
    };
}

#endif