#ifndef __UTILS_H__
#define __UTILS_H__

#include <vector>
#include <ranges>

template<typename T> class not_null;
template<typename T> class not_null<T *> {
public:
    not_null() = default;
    not_null(std::nullptr_t) { check(); }
    not_null(T *value) : value(value) { check(); }

    operator T *() { return value; }
    operator T *() const { return value; }

    T *get() { return value; }
    T *get() const { return value; }

    T &operator *() { check(); return *value; }
    T &operator *() const { check(); return *value; }

    T *operator -> () { check(); return value; }
    T *operator -> () const { check(); return value; }

private:
    void check() const {
        if (!value) throw std::runtime_error("value can not be null");
    }

private:
    T *value = nullptr;
};

template<typename T> not_null(T *) -> not_null<T *>;

inline auto to_vector(std::ranges::range auto &&range) {
    std::vector<std::ranges::range_value_t<decltype(range)>> ret;
    if constexpr(std::ranges::sized_range<decltype(range)>) {
        ret.reserve(std::ranges::size(range));
    }
    std::ranges::copy(range, std::back_inserter(ret));
    return ret;
}

inline auto to_vector_not_null(std::ranges::range auto &&range) {
    return to_vector(range | std::views::transform([](auto *value) {
        return not_null(value);
    }));
}

inline auto unwrap_vector_not_null(std::ranges::range auto &&range) {
    return to_vector(range | std::views::transform([]<typename T>(not_null<T *> value) -> T * {
        return value;
    }));
}

template<std::ranges::input_range R, typename T, typename Proj = std::identity>
requires std::indirect_binary_predicate<std::ranges::equal_to, std::projected<std::ranges::iterator_t<R>, Proj>, const T *>
constexpr bool ranges_contains(R &&r, const T &value, Proj proj = {}) {
    return std::ranges::find(r, value, proj) != std::ranges::end(r);
}

template<typename ... Ts> struct overloaded : Ts ... { using Ts::operator() ...; };
template<typename ... Ts> overloaded(Ts ...) -> overloaded<Ts ...>;

template<typename T> struct same_if_trivial { using type = const T &; };
template<typename T> using same_if_trivial_t = typename same_if_trivial<T>::type;

template<typename T> requires (std::is_trivially_copyable_v<T>)
struct same_if_trivial<T> { using type = T; };

#endif