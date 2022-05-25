#ifndef __ENUM_VARIANT_H__
#define __ENUM_VARIANT_H__

#include <variant>
#include <functional>

#include "enums.h"
#include "type_list.h"

namespace enums {

    namespace detail {
        template<typename T> struct type_or_monostate { using type = T; };
        template<> struct type_or_monostate<void> { using type = std::monostate; };

        template<reflected_enum Enum, typename EnumSeq, template<Enum> typename Transform>
        struct make_enum_variant;

        template<reflected_enum Enum, Enum ... Es, template<Enum> typename Transform>
        struct make_enum_variant<Enum, enum_sequence<Es ...>, Transform> {
            using type = std::variant<typename type_or_monostate<typename Transform<Es>::type>::type ... >;
        };
    }
    
    template<reflected_enum auto E>
    struct enum_type_or_void { using type = void; };
    
    template<reflected_enum auto E> requires value_with_type<E>
    struct enum_type_or_void<E> { using type = enum_type_t<E>; };

    template<reflected_enum Enum, template<Enum> typename Transform = enum_type_or_void>
    struct enum_variant : detail::make_enum_variant<Enum, make_enum_sequence<Enum>, Transform>::type {
        using base = typename detail::make_enum_variant<Enum, make_enum_sequence<Enum>, Transform>::type;
        using enum_type = Enum;

        template<Enum E> using value_type = typename Transform<E>::type;

        using base::variant;

        template<Enum Value, typename ... Ts>
        enum_variant(enum_tag_t<Value>, Ts && ... args)
            : base(std::in_place_index<indexof(Value)>, std::forward<Ts>(args) ...) {}

        base &variant_base() {
            return static_cast<base &>(*this);
        }

        const base &variant_base() const {
            return static_cast<const base &>(*this);
        }

        template<Enum Value, typename ... Ts>
        auto &emplace(Ts && ... args) {
            return base::template emplace<indexof(Value)>(std::forward<Ts>(args) ...);
        }
        
        Enum enum_index() const {
            return index_to<Enum>(base::index());
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
    };

    template<typename T> struct is_variant : std::false_type {};
    template<typename ... Ts> struct is_variant<std::variant<Ts ...>> : std::true_type {};

    template<typename T>
    concept is_enum_variant = requires {
        requires reflected_enum<typename T::enum_type>;
        requires is_variant<typename T::base>::value;
    };
    
    namespace detail {
        template<typename T> struct variant_type_list{};
        template<typename T> using variant_type_list_t = typename variant_type_list<T>::type;

        template<typename ... Ts> struct variant_type_list<std::variant<Ts...>> {
            using type = util::type_list<Ts...>;
        };
    }

    template<typename RetType, typename Visitor, typename Variant>
    requires is_enum_variant<std::remove_const_t<Variant>>
    RetType do_visit(Visitor &&visitor, Variant &v) {
        return visit_enum<RetType>([&](auto tag) {
            if constexpr (!std::is_void_v<typename Variant::value_type<tag.value>>) {
                return std::invoke(visitor, tag, v.template get<tag.value>());
            } else {
                return std::invoke(visitor, tag);
            }
        }, v.enum_index());
    }

    template<typename RetType, typename Visitor, typename Variant>
    requires is_enum_variant<std::remove_const_t<Variant>>
    RetType visit_indexed(Visitor &&visitor, Variant &v) {
        return do_visit<RetType>(visitor, v);
    }

    template<typename Visitor, typename Variant, reflected_enum auto E>
    struct visit_return_type : std::invoke_result<Visitor, enum_tag_t<E>> {};

    template<typename Visitor, typename Variant, reflected_enum auto E>
    requires (!std::is_void_v<typename Variant::value_type<E>>)
    struct visit_return_type<Visitor, Variant, E>
        : std::invoke_result<Visitor, enum_tag_t<E>,
            std::add_lvalue_reference_t<typename Variant::value_type<E>>> {};

    template<typename Visitor, typename Variant, reflected_enum auto E>
    using visit_return_type_t = typename visit_return_type<Visitor, Variant, E>::type;

    template<typename Visitor, typename Variant>
    requires is_enum_variant<std::remove_const_t<Variant>>
    decltype(auto) visit_indexed(Visitor &&visitor, Variant &v) {
        using enum_type = typename Variant::enum_type;
        return do_visit<visit_return_type_t<Visitor, Variant, enum_type{}>>(visitor, v);
    }

    template<typename RetType, typename Visitor, typename Variant>
    requires is_enum_variant<std::remove_const_t<Variant>>
    RetType visit(Visitor &&visitor, Variant &v) {
        return std::visit<RetType>(visitor, v.variant_base());
    }

    template<typename Visitor, typename Variant>
    requires is_enum_variant<std::remove_const_t<Variant>>
    decltype(auto) visit(Visitor &&visitor, Variant &v) {
        return std::visit(visitor, v.variant_base());
    }
}

#endif