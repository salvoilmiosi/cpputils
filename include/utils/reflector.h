#ifndef __REFLECTOR_H__
#define __REFLECTOR_H__

#include <boost/preprocessor.hpp>
#include <utility>
#include <cstddef>

#include "type_list.h"

#define HELPER1(...) ((__VA_ARGS__)) HELPER2
#define HELPER2(...) ((__VA_ARGS__)) HELPER1
#define HELPER1_END
#define HELPER2_END
#define ADD_PARENTHESES(sequence) BOOST_PP_CAT(HELPER1 sequence,_END)

#define REFLECT_DECLARE_FIELD_IMPL(type, name, ...) type name{__VA_ARGS__}
#define REFLECT_DECLARE_FIELD(tuple) REFLECT_DECLARE_FIELD_IMPL tuple

#define ARGTYPE(tuple) BOOST_PP_TUPLE_ELEM(0, tuple)
#define ARGNAME(tuple) BOOST_PP_TUPLE_ELEM(1, tuple)

#define REFLECT_EACH(r, data, i, x) \
REFLECT_DECLARE_FIELD(x); \
template<class Self> struct reflector_field_data<i, Self> { \
    static constexpr size_t index = i; \
    using self_type = Self; \
    Self &self; \
    constexpr reflector_field_data(Self &self) : self(self) {} \
    auto &get() { return self.ARGNAME(x); } \
    const auto &get() const { return self.ARGNAME(x); } \
    static constexpr const char *name() { return BOOST_PP_STRINGIZE(ARGNAME(x)); } \
};

#define REFLECTABLE_IMPL(elementTupleSeq) \
struct reflector_num_fields { \
    static constexpr size_t value = BOOST_PP_SEQ_SIZE(elementTupleSeq); \
}; \
template<size_t N, class Self> \
struct reflector_field_data {}; \
BOOST_PP_SEQ_FOR_EACH_I(REFLECT_EACH, data, elementTupleSeq)

#define REFLECTABLE(elements) REFLECTABLE_IMPL(ADD_PARENTHESES(elements))

#define DEFINE_STRUCT(structName, elements, ...) struct structName{REFLECTABLE(elements) __VA_ARGS__};

namespace reflector {
    namespace detail {
        template<typename T>
        concept is_field_data = requires(T &t) {
            t.get();
            T::name();
        };

        template<typename T, size_t I>
        static constexpr bool has_field_data_v = requires(T &t) {
            typename T::template reflector_field_data<I, T>;
            { typename T::template reflector_field_data<I, T>(t) } -> is_field_data;
        };

        template<typename T, size_t ... Is>
        constexpr bool check_has_field_data(std::index_sequence<Is...>) {
            return (has_field_data_v<T, Is> && ...);
        };
    }

    template<typename T>
    concept reflectable = requires {
        typename T::reflector_num_fields;
        requires detail::check_has_field_data<T>(std::make_index_sequence<T::reflector_num_fields::value>());
    };

    template<reflectable ... Ts> struct reflectable_base : Ts ... {
        using reflector_base_types = util::type_list<Ts ...>;
    };

    namespace detail {
        template<typename T, typename TList> struct is_derived_from_all {};

        template<typename T, typename ... Ts>
        struct is_derived_from_all<T, util::type_list<Ts ...>> {
            static constexpr bool value = (std::is_base_of_v<Ts, T> && ...);
        };

        template<typename T, typename TList> concept derived_from_all = is_derived_from_all<T, TList>::value;

        template<typename T> concept has_reflectable_base = requires {
            typename T::reflector_base_types;
            requires derived_from_all<T, typename T::reflector_base_types>;
        };

        template<reflectable T> struct num_fields {
            static constexpr size_t value = T::reflector_num_fields::value;
        };

        template<typename TList> struct sum_num_fields{};
        template<typename ... Ts> struct sum_num_fields<util::type_list<Ts ...>> {
            static constexpr size_t value = (num_fields<Ts>::value + ...);
        };

        template<reflectable T> requires has_reflectable_base<T>
        struct num_fields<T> {
            static constexpr size_t value = T::reflector_num_fields::value +
                sum_num_fields<typename T::reflector_base_types>::value;
        };

        template<size_t I, typename Derived, typename TList> struct impl_field_data {};
        template<size_t I, typename Derived> struct impl_field_data<I, Derived, util::type_list<>> {
            using type = typename Derived::template reflector_field_data<I, Derived>;
        };

        template<size_t I, typename Derived, typename First, typename ... Ts>
        requires (I < num_fields<First>::value)
        struct impl_field_data<I, Derived, util::type_list<First, Ts...>> {
            using type = typename First::template reflector_field_data<I, First>;
        };

        template<size_t I, typename Derived, typename First, typename ... Ts>
        requires (I >= num_fields<First>::value)
        struct impl_field_data<I, Derived, util::type_list<First, Ts...>>
            : impl_field_data<I - num_fields<First>::value, Derived, util::type_list<Ts ...>> {};

        template<size_t I, reflectable T> struct field_data : impl_field_data<I, T, util::type_list<>> {};
        template<size_t I, reflectable T> requires has_reflectable_base<T>
        struct field_data<I, T> : impl_field_data<I, T, typename T::reflector_base_types> {};
    }

    template<reflectable T> constexpr size_t num_fields = detail::num_fields<T>::value;

    template<size_t I, reflectable T>
    constexpr auto get_field_name() {
        return detail::field_data<I, T>::type::name();
    }

    template<size_t I, reflectable T>
    constexpr auto get_field_data(T &x) {
        return typename detail::field_data<I, T>::type(x);
    }

    template<size_t I, reflectable T>
    constexpr auto get_field_data(const T &x) {
        using field_data = typename detail::field_data<I, T>::type;
        using self_type = typename field_data::self_type;
        return typename self_type::reflector_field_data<field_data::index, std::add_const_t<self_type>>(x);
    }

    template<typename T>
    struct to_tuple_converter;

    template<reflectable T>
    struct to_tuple_converter<T> {
        template<std::convertible_to<T> U, size_t ... Is>
        auto to_tuple_helper(U &&value, std::index_sequence<Is...>) const {
            return std::tie(get_field_data<Is, T>(value).get() ...);
        }

        template<std::convertible_to<T> U>
        auto operator()(U &&value) const {
            return to_tuple_helper(value, std::make_index_sequence<num_fields<T>>());
        }
    };

    template<typename T>
    using to_tuple_converter_decay = to_tuple_converter<std::decay_t<T>>;

    template<typename T>
    concept is_tuple = requires {
        { std::tuple_size<T>::value } -> std::convertible_to<size_t>;
    };

    template<typename T>
    concept convertible_to_tuple = requires {
        typename to_tuple_converter_decay<T>;
        requires std::invocable<to_tuple_converter_decay<T>, T>;
        requires is_tuple<std::invoke_result_t<to_tuple_converter_decay<T>, T>>;
    };

    template<convertible_to_tuple T>
    auto as_tuple(T &&value) {
        return to_tuple_converter_decay<T>{}(std::forward<T>(value));
    }

    template<convertible_to_tuple T>
    using as_tuple_t = std::invoke_result_t<to_tuple_converter_decay<T>, T>;
    
}


#endif