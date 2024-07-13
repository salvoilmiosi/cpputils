#ifndef __ENUM_VARIANT_H__
#define __ENUM_VARIANT_H__

#include <variant>
#include <functional>

#include "enums.h"

namespace enums {

    template<enumeral auto E> struct tag_t { static constexpr auto value = E; };
    template<enumeral auto E> constexpr tag_t<E> tag;

    template<enumeral auto Enum, typename T>
    struct type_assoc {
        static constexpr auto enum_value = Enum;
        using enum_type = T;
    };

    namespace detail {
        template<typename ... Ts> struct type_map {
            static constexpr size_t size = sizeof...(Ts);
        };

        template<enumeral auto Enum, typename TMap>
        struct type_map_contains;

        template<enumeral auto Enum, typename TFirst, typename ... Rest> requires (TFirst::enum_value != Enum)
        struct type_map_contains<Enum, type_map<TFirst, Rest...>> : type_map_contains<Enum, type_map<Rest...>> {};

        template<enumeral auto Enum, typename TFirst, typename ... Rest> requires (TFirst::enum_value == Enum)
        struct type_map_contains<Enum, type_map<TFirst, Rest...>> : std::true_type {};

        template<enumeral auto Enum>
        struct type_map_contains<Enum, type_map<>> : std::false_type {};

        template<enumeral auto Enum, typename TMap>
        struct type_map_get;

        template<enumeral auto Enum, typename TFirst, typename ... Rest> requires (TFirst::enum_value != Enum)
        struct type_map_get<Enum, type_map<TFirst, Rest...>> : type_map_get<Enum, type_map<Rest...>> {};

        template<enumeral auto Enum, typename TFirst, typename ... Rest> requires (TFirst::enum_value == Enum)
        struct type_map_get<Enum, type_map<TFirst, Rest...>> : std::type_identity<typename TFirst::enum_type> {};

        template<enumeral auto Enum>
        struct type_map_get<Enum, type_map<>> : std::type_identity<std::monostate> {};

        template<typename ESeq, typename TMap>
        struct make_enum_variant;
        
        template<typename TMap, enumeral auto ... Es>
        struct make_enum_variant<enum_sequence<Es ...>, TMap> {
            using type = std::variant<typename type_map_get<Es, TMap>::type ...>;
        };
    }

    template<enumeral Enum, typename ... Assocs>
    struct enum_variant : detail::make_enum_variant<make_enum_sequence<Enum>, detail::type_map<Assocs...>>::type {
        using base = typename detail::make_enum_variant<make_enum_sequence<Enum>, detail::type_map<Assocs...>>::type;
        using enum_type = Enum;

        template<Enum E> static constexpr bool has_type = detail::type_map_contains<E, detail::type_map<Assocs...>>::value;
        template<Enum E> requires (has_type<E>) using value_type = detail::type_map_get<E, detail::type_map<Assocs...>>::type;

        using base::variant;

        template<Enum Value>
        enum_variant(tag_t<Value>, auto && ... args)
            : base(std::in_place_index<indexof(Value)>, std::forward<decltype(args)>(args) ...) {}

        base &variant_base() {
            return static_cast<base &>(*this);
        }

        const base &variant_base() const {
            return static_cast<const base &>(*this);
        }

        template<Enum Value>
        auto &emplace(auto && ... args) {
            return base::template emplace<indexof(Value)>(std::forward<decltype(args)>(args) ...);
        }
        
        Enum enum_index() const {
            return enums::index_to<Enum>(base::index());
        }

        bool is(Enum index) const {
            return enum_index() == index;
        }

        template<Enum Value> const auto &get() const {
            return std::get<indexof(Value)>(*this);
        }

        template<Enum Value> auto &get() {
            return std::get<indexof(Value)>(*this);
        }

        template<Enum Value> const auto *get_if() const {
            return std::get_if<indexof(Value)>(this);
        }

        template<Enum Value> auto *get_if() {
            return std::get_if<indexof(Value)>(this);
        }
    };

    template<typename T> struct is_variant : std::false_type {};
    template<typename ... Ts> struct is_variant<std::variant<Ts ...>> : std::true_type {};

    template<typename T>
    concept is_enum_variant = requires {
        requires enumeral<typename T::enum_type>;
        requires is_variant<typename T::base>::value;
    };
    
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
                return std::invoke(std::forward<Visitor>(visitor), enums::tag<index_to<Enums>(Indices)> ... );
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
        using result_type = std::invoke_result_t<Visitor, tag_t<Enums{}> ... >;
        return visit_enum<result_type>(std::forward<Visitor>(visitor), values ... );
    }

    template<typename RetType, typename Visitor, typename Variant>
    requires is_enum_variant<std::remove_cvref_t<Variant>>
    RetType do_visit(Visitor &&visitor, Variant &&v) {
        using variant_type = std::remove_cvref_t<Variant>;
        using enum_type = typename variant_type::enum_type;
        return visit_enum<RetType>([&]<enum_type E>(enums::tag_t<E> tag) {
            if constexpr (variant_type::template has_type<E>) {
                return std::invoke(visitor, tag, v.template get<E>());
            } else {
                return std::invoke(visitor, tag);
            }
        }, v.enum_index());
    }

    template<typename RetType, typename Visitor, typename Variant>
    requires is_enum_variant<std::remove_cvref_t<Variant>>
    RetType visit_indexed(Visitor &&visitor, Variant &&v) {
        return do_visit<RetType>(visitor, v);
    }

    template<typename Visitor, typename Variant, enumeral auto E>
    struct visit_return_type : std::invoke_result<Visitor, tag_t<E>> {};

    template<typename Visitor, typename Variant, enumeral auto E>
    requires (Variant::template has_type<E>)
    struct visit_return_type<Visitor, Variant, E> {
        using type = decltype(std::invoke(
            std::declval<Visitor>(), tag<E>,
            std::declval<typename Variant::template value_type<E>>()
        ));
    };

    template<typename Visitor, typename Variant, enumeral auto E>
    using visit_return_type_t = typename visit_return_type<Visitor, Variant, E>::type;

    template<typename Visitor, typename Variant>
    requires is_enum_variant<std::remove_cvref_t<Variant>>
    decltype(auto) visit_indexed(Visitor &&visitor, Variant &&v) {
        using variant_type = std::remove_cvref_t<Variant>;
        using enum_type = typename variant_type::enum_type;
        return do_visit<visit_return_type_t<Visitor, variant_type, enum_type{}>>(visitor, v);
    }

    template<typename RetType, typename Visitor, typename Variant>
    requires is_enum_variant<std::remove_cvref_t<Variant>>
    RetType visit(Visitor &&visitor, Variant &&v) {
        return std::visit<RetType>(visitor, v.variant_base());
    }

    template<typename Visitor, typename Variant>
    requires is_enum_variant<std::remove_cvref_t<Variant>>
    decltype(auto) visit(Visitor &&visitor, Variant &&v) {
        return std::visit(visitor, v.variant_base());
    }
}

#endif