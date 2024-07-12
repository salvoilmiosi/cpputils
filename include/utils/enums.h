#ifndef __ENUMS_H__
#define __ENUMS_H__

#include <magic_enum/magic_enum.hpp>

#include <stdexcept>
#include <functional>
#include <algorithm>
#include <optional>
#include <string>
#include <ranges>
#include <limits>
#include <tuple>
#include <array>
#include <bit>

namespace enums {

    template<typename T> concept enumeral = magic_enum::is_scoped_enum_v<T>;

    template<enumeral auto E> struct enum_tag_t { static constexpr auto value = E; };
    template<enumeral auto E> constexpr enum_tag_t<E> enum_tag;

    template<enumeral T> constexpr std::string_view enum_name_v = magic_enum::enum_type_name<T>();

    template<enumeral T> constexpr auto enum_values_v = magic_enum::enum_values<T>();

    template<enumeral T> constexpr size_t num_members_v = magic_enum::enum_count<T>();

    template<enumeral T>
    constexpr std::string_view to_string(T value) {
        return magic_enum::enum_name(value);
    }

    template<enumeral T>
    constexpr std::optional<T> from_string(std::string_view str) {
        return magic_enum::enum_cast<T>(str);
    }

    template<enumeral T>
    constexpr size_t indexof(T value) {
        if (auto result = magic_enum::enum_index(value)) {
            return *result;
        }
        throw std::out_of_range("invalid enum index");
    }

    template<enumeral T>
    constexpr T index_to(size_t index) {
        return magic_enum::enum_value<T>(index);
    }

    template<enumeral auto ... Values> struct enum_sequence {
        static constexpr size_t size = sizeof...(Values);
    };

    namespace detail {
        template<enumeral T, typename ISeq> struct make_enum_sequence{};
        template<enumeral T, size_t ... Is> struct make_enum_sequence<T, std::index_sequence<Is...>> {
            using type = enum_sequence<index_to<T>(Is)...>;
        };
    }

    template<enumeral T> using make_enum_sequence = typename detail::make_enum_sequence<T, std::make_index_sequence<num_members_v<T>>>::type;

    namespace detail {
        template<typename Function, size_t ... Dimensions>
        struct multi_array;

        template<typename Function>
        struct multi_array<Function> {
            Function value;

            constexpr const Function &get() const {
                return value;
            }
        };

        template<typename Function, size_t First, size_t ... Rest>
        struct multi_array<Function, First, Rest...> {
            multi_array<Function, Rest ...> value[First];

            constexpr decltype(auto) get(size_t first_index, auto ... rest_indices) const {
                return value[first_index].get(rest_indices ... );
            }
        };

        template<typename ArrayType, typename IndexSeq>
        struct gen_vtable_impl;

        template<typename RetType, typename Visitor, size_t ... Dimensions, enumeral ... Enums, size_t ... Indices>
        struct gen_vtable_impl<multi_array<RetType (*)(Visitor, Enums...), Dimensions...>, std::index_sequence<Indices...>> {
            using array_type = multi_array<RetType (*)(Visitor, Enums...), Dimensions...>;
            using next_enum = std::tuple_element_t<sizeof...(Indices), std::tuple<Enums...>>;

            static constexpr array_type apply() {
                array_type vtable{};
                apply_all(vtable, std::make_index_sequence<enums::num_members_v<next_enum>>());
                return vtable;
            }

            template<size_t ... Is>
            static constexpr void apply_all(array_type &vtable, std::index_sequence<Is...>) {
                (apply_single<Is>(vtable.value[Is]), ...);
            }

            template<size_t Index>
            static constexpr void apply_single(auto &elem) {
                elem = gen_vtable_impl<std::remove_reference_t<decltype(elem)>, std::index_sequence<Indices..., Index>>::apply();
            }
        };

        template<typename RetType, typename Visitor, enumeral ... Enums, size_t ... Indices>
        struct gen_vtable_impl<multi_array<RetType (*)(Visitor, Enums...)>, std::index_sequence<Indices...>> {
            using array_type = multi_array<RetType (*)(Visitor, Enums...)>;

            static constexpr RetType visit_invoke(Visitor &&visitor, Enums ... enums) {
                return std::invoke(std::forward<Visitor>(visitor), enums::enum_tag<index_to<Enums>(Indices)> ... );
            }

            static constexpr auto apply() {
                return array_type{&visit_invoke};
            }
        };

        template<typename RetType, typename Visitor, enumeral ... Enums>
        struct gen_vtable {
            using array_type = multi_array<RetType (*)(Visitor, Enums...), enums::num_members_v<Enums> ... >;

            static constexpr array_type value = gen_vtable_impl<array_type, std::index_sequence<>>::apply();
        };
    }
    
    template<typename RetType, typename Visitor, enumeral ... Enums>
    RetType visit_enum(Visitor &&visitor, Enums ... values) {
        return detail::gen_vtable<RetType, Visitor &&, Enums ... >::value.get(indexof(values) ... )
            (std::forward<Visitor>(visitor), values ...);
    }

    template<typename Visitor, enumeral ... Enums>
    decltype(auto) visit_enum(Visitor &&visitor, Enums ... values) {
        using result_type = std::invoke_result_t<Visitor, enum_tag_t<Enums{}> ... >;
        return visit_enum<result_type>(std::forward<Visitor>(visitor), values ... );
    }
    
}

#endif