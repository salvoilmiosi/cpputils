#ifndef __UTILS_H__
#define __UTILS_H__

#include <vector>
#include <ranges>

inline auto to_vector(std::ranges::range auto &&range) {
    std::vector<std::ranges::range_value_t<decltype(range)>> ret;
    if constexpr(std::ranges::sized_range<decltype(range)>) {
        ret.reserve(std::ranges::size(range));
    }
    std::ranges::copy(range, std::back_inserter(ret));
    return ret;
}

template<typename ... Ts> struct overloaded : Ts ... { using Ts::operator() ...; };
template<typename ... Ts> overloaded(Ts ...) -> overloaded<Ts ...>;

template<typename T> struct same_if_trivial { using type = const T &; };
template<typename T> using same_if_trivial_t = typename same_if_trivial<T>::type;

template<typename T> requires (std::is_trivially_copyable_v<T>)
struct same_if_trivial<T> { using type = T; };

#endif