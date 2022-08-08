#ifndef __ENUM_VARIANT_H__
#define __ENUM_VARIANT_H__

#include <variant>
#include <functional>

#include "enums.h"
#include "type_list.h"

namespace enums {

    namespace detail {
        template<reflected_enum Enum, template<reflected_enum auto> typename Transform, Enum Value, class = void>
        struct transform_has_type : std::false_type {};

        template<reflected_enum Enum, template<reflected_enum auto> typename Transform, Enum Value>
        struct transform_has_type<Enum, Transform, Value, std::void_t<typename Transform<Value>::type>> : std::true_type {};

        template<reflected_enum Enum, template<reflected_enum auto> typename Transform, Enum Value>
        struct transform_get {
            using type = std::monostate;
        };

        template<reflected_enum Enum, template<reflected_enum auto> typename Transform, Enum Value>
        requires (transform_has_type<Enum, Transform, Value>::value)
        struct transform_get<Enum, Transform, Value> {
            using type = typename Transform<Value>::type;
        };

        template<reflected_enum Enum, template<reflected_enum auto> typename Transform, typename EnumSeq>
        struct make_enum_variant;

        template<reflected_enum Enum, template<reflected_enum auto> typename Transform, Enum ... Es>
        struct make_enum_variant<Enum, Transform, enum_sequence<Es ...>> {
            using type = std::variant<typename transform_get<Enum, Transform, Es>::type ... >;
        };
    }

    template<reflected_enum Enum, template<reflected_enum auto> typename Transform = enum_type>
    struct enum_variant : detail::make_enum_variant<Enum, Transform, make_enum_sequence<Enum>>::type {
        using base = typename detail::make_enum_variant<Enum, Transform, make_enum_sequence<Enum>>::type;
        using enum_type = Enum;

        template<Enum E> static constexpr bool has_type = detail::transform_has_type<Enum, Transform, E>::value;
        template<Enum E> requires (has_type<E>) using value_type = typename Transform<E>::type;

        using base::variant;

        template<Enum Value>
        enum_variant(enum_tag_t<Value>, auto && ... args)
            : base(std::in_place_index<indexof(Value)>, FWD(args) ...) {}

        base &variant_base() {
            return static_cast<base &>(*this);
        }

        const base &variant_base() const {
            return static_cast<const base &>(*this);
        }

        template<Enum Value>
        auto &emplace(auto && ... args) {
            return base::template emplace<indexof(Value)>(FWD(args) ...);
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
        using enum_type = typename Variant::enum_type;
        return visit_enum<RetType>([&]<enum_type E>(enums::enum_tag_t<E> tag) {
            if constexpr (Variant::template has_type<E>) {
                return std::invoke(visitor, tag, v.template get<E>());
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
    requires (Variant::template has_type<E>)
    struct visit_return_type<Visitor, Variant, E> {
        using type = decltype(std::invoke(
            std::declval<Visitor>(), enum_tag<E>,
            std::declval<typename Variant::template value_type<E>>()
        ));
    };

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

#define VARIANT_TRANSFORM_NAME(variantName) __##variantName##__transform

#define GENERATE_TRANSFORM_CASE_IMPL(variantName, enumName, enumValue, enumType) \
    template<> struct VARIANT_TRANSFORM_NAME(variantName)<enumName::enumValue> { \
        using type = enumType; \
    };

#define GENERATE_TRANSFORM_CASE(r, variantName_enumName, elementTuple) \
    GENERATE_TRANSFORM_CASE_IMPL( \
        BOOST_PP_TUPLE_ELEM(0, variantName_enumName), \
        BOOST_PP_TUPLE_ELEM(1, variantName_enumName), \
        BOOST_PP_TUPLE_ELEM(0, elementTuple), \
        BOOST_PP_TUPLE_ELEM(1, elementTuple) \
    )

#define DEFINE_ENUM_VARIANT_IMPL(variant_name, enum_type, element_tuple_seq) \
    template<enum_type E> struct VARIANT_TRANSFORM_NAME(variant_name); \
    BOOST_PP_SEQ_FOR_EACH(GENERATE_TRANSFORM_CASE, (variant_name, enum_type), element_tuple_seq) \
    using variant_name = enums::enum_variant<enum_type, VARIANT_TRANSFORM_NAME(variant_name)>;

#define DEFINE_ENUM_VARIANT(variant_name, enum_type, transformElements) \
    DEFINE_ENUM_VARIANT_IMPL(variant_name, enum_type, ADD_PARENTHESES(transformElements))

#endif