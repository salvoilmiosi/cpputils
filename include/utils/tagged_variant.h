#ifndef __TAGGED_VARIANT_H__
#define __TAGGED_VARIANT_H__

#include <variant>

#include "tstring.h"
#include "json_serial.h"

namespace utils {
    
    template<tstring Name, typename T = void>
    struct tag {
        static constexpr auto name = Name;
        using type = T;
    };

    template<typename T>
    concept is_tag = requires {
        { T::name } -> std::convertible_to<std::string_view>;
        typename T::type;
    };

    namespace detail {
        template<typename ... Ts>
        constexpr bool check_unique_names() {
            constexpr size_t size = sizeof...(Ts);
            constexpr std::string_view names[size] = { Ts::name ... };
            for (size_t i=0; i<size-1; ++i) {
                for (size_t j=i+1; j<size; ++j) {
                    if (names[i] == names[j]) {
                        return false;
                    }
                }
            }
            return true;
        }

        template<typename T> using type_or_monostate = std::conditional_t<std::is_void_v<T>, std::monostate, T>;

        template<typename ... Ts>
        struct build_tagged_variant {
            static_assert(check_unique_names<Ts ...>(), "Tag names must be unique");
            using type = std::variant<type_or_monostate<typename Ts::type> ...>;
        };

        template<typename Variant, typename Tag> struct find_tag_name;
    }

    template<typename ... Ts>
    struct tagged_variant : detail::build_tagged_variant<Ts ...>::type {
        using base = detail::build_tagged_variant<Ts ...>::type;
        using base::variant;

        template<is_tag Tag, typename ... Args>
        tagged_variant(Tag, Args && ... args)
            : base(std::in_place_index<detail::find_tag_name<tagged_variant, Tag>::index>, std::forward<Args>(args) ...) {}
    };

    namespace detail {
        template<typename ... Ts>
        constexpr size_t find_tag_name_impl(std::string_view name) {
            constexpr size_t size = sizeof...(Ts);
            constexpr std::string_view names[size] = { Ts::name ... };
            for (size_t i=0; i<size; ++i) {
                if (names[i] == name) {
                    return i;
                }
            }
            throw "Cannot find name";
        }

        template<typename Tag, typename ... Ts>
        struct find_tag_name<tagged_variant<Ts ...>, Tag> {
            static constexpr size_t index = find_tag_name_impl<Ts ...>(Tag::name);
        };

        template<typename Variant, size_t I> struct tagged_variant_type_at;

        template<size_t I, typename ... Ts>
        struct tagged_variant_type_at<tagged_variant<Ts ...>, I> {
            using type = std::tuple_element_t<I, std::tuple<typename Ts::type ...>>;
        };

        template<typename T> struct is_tagged_variant : std::false_type {};
        template<typename ... Ts> struct is_tagged_variant<tagged_variant<Ts ...>> : std::true_type {};
    }

    template<typename T> struct tagged_variant_tag_names;

    template<typename ... Ts>
    struct tagged_variant_tag_names<tagged_variant<Ts ...>> {
        static constexpr std::array value { std::string_view(Ts::name) ... };
    };

    template<typename T>
    concept is_tagged_variant = detail::is_tagged_variant<T>::value;

    template<typename Tag, typename Variant>
    concept tag_for = requires {
        requires is_tag<Tag>;
        requires is_tagged_variant<Variant>;
        detail::find_tag_name<Variant, Tag>::index;
    };

    template<typename Variant, typename Tag> requires tag_for<Tag, Variant>
    using tagged_variant_value_type = typename detail::tagged_variant_type_at<Variant,
        detail::find_tag_name<Variant, Tag>::index
    >::type;

    template<tstring Name, typename Variant> requires tag_for<tag<Name>, std::remove_cvref_t<Variant>>
    decltype(auto) get(Variant &&variant) {
        return std::get<detail::find_tag_name<std::remove_cvref_t<Variant>, tag<Name>>::index>(variant);
    }
    
    template<typename Variant>
    class tagged_variant_index {
    private:
        size_t m_index;

    public:
        constexpr tagged_variant_index() = default;

        explicit constexpr tagged_variant_index(const Variant &variant)
            : m_index{variant.index()} {}

        explicit constexpr tagged_variant_index(tag_for<Variant> auto tag)
            : m_index{detail::find_tag_name<Variant, decltype(tag)>::index} {}
        
        explicit constexpr tagged_variant_index(std::string_view key) {
            const auto &tag_names = utils::tagged_variant_tag_names<Variant>::value;
            for (size_t i=0; i<tag_names.size(); ++i) {
                if (tag_names[i] == key) {
                    m_index = i;
                    return;
                }
            }
            throw std::runtime_error(fmt::format("Invalid variant type: {}", key));
        }

        constexpr bool operator == (const tagged_variant_index &other) const = default;

        constexpr size_t index() const {
            return m_index;
        }

        constexpr std::string_view to_string() const {
            return utils::tagged_variant_tag_names<Variant>::value[index()];
        }
    };
    
    template<typename Visitor, typename Variant> struct visit_return_type;

    template<typename Visitor, tstring First, typename T, typename ... Ts>
    struct visit_return_type<Visitor, tagged_variant<tag<First, T>, Ts ...>> : std::invoke_result<Visitor, tag<First>, T> {};

    template<typename Visitor, tstring First, typename ... Ts>
    struct visit_return_type<Visitor, tagged_variant<tag<First>, Ts ...>> : std::invoke_result<Visitor, tag<First>> {};

    template<typename RetType, typename Visitor, typename ... Ts>
    RetType visit_tagged(Visitor &&visitor, tagged_variant_index<tagged_variant<Ts ...>> index) {
        static constexpr std::array<RetType (*)(Visitor&&), sizeof...(Ts)> vtable {
            [](Visitor &&visitor) -> RetType {
                return std::invoke(std::forward<Visitor>(visitor), tag<Ts::name>{});
            } ...
        };
        return vtable[index.index()](std::forward<Visitor>(visitor));
    }

    template<typename Visitor, typename ... Ts>
    decltype(auto) visit_tagged(Visitor &&visitor, tagged_variant_index<tagged_variant<Ts ...>> index) {
        using return_type = typename visit_return_type<Visitor, tagged_variant<tag<Ts::name> ...>>::type;
        return visit_tagged<return_type>(std::forward<Visitor>(visitor), index);
    }

    template<typename RetType, typename Visitor, typename Variant> requires is_tagged_variant<std::remove_cvref_t<Variant>>
    RetType visit_tagged(Visitor &&visitor, Variant &&variant) {
        using variant_type = std::remove_cvref_t<Variant>;
        return visit_tagged<RetType>([&](tag_for<variant_type> auto tag) -> RetType {
            if constexpr (std::is_void_v<tagged_variant_value_type<variant_type, decltype(tag)>>) {
                return std::invoke(std::forward<Visitor>(visitor), tag);
            } else {
                return std::invoke(std::forward<Visitor>(visitor), tag, get<tag.name>(std::forward<Variant>(variant)));
            }
        }, tagged_variant_index(variant));
    }

    template<typename Visitor, typename Variant> requires is_tagged_variant<std::remove_cvref_t<Variant>>
    decltype(auto) visit_tagged(Visitor &&visitor, Variant &&variant) {
        using return_type = typename visit_return_type<Visitor, std::remove_cvref_t<Variant>>::type;
        return visit_tagged<return_type>(std::forward<Visitor>(visitor), std::forward<Variant>(variant));
    }
}

namespace json {

    template<typename Context, typename ... Ts>
    struct serializer<utils::tagged_variant_index<utils::tagged_variant<Ts ...>>, Context> {
        json operator()(const utils::tagged_variant_index<utils::tagged_variant<Ts ...>> &value) const {
            return std::string(value.to_string());
        }
    };

    template<typename Context, typename ... Ts>
    struct deserializer<utils::tagged_variant_index<utils::tagged_variant<Ts ...>>, Context> {
        using value_type = utils::tagged_variant_index<utils::tagged_variant<Ts ...>>;
        value_type operator()(const json &value) const {
            if (!value.is_string()) {
                throw std::runtime_error("Cannot deserialize tagged variant index: value is not a string");
            }
            return value_type{std::string_view(value.get<std::string>())};
        }
    };

    template<typename T, typename Context>
    concept void_or_serializable = std::is_void_v<T> || serializable<T, Context>;
    
    template<typename Context, typename ... Ts> requires (void_or_serializable<typename Ts::type, Context> && ...)
    struct serializer<utils::tagged_variant<Ts ...>, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;

        using variant_type = utils::tagged_variant<Ts ...>;

        template<typename T>
        json serialize_args(T &&arg) const {
            return this->serialize_with_context(std::forward<T>(arg));
        }

        json serialize_args() const {
            return json::object();
        }
        
        json operator()(const variant_type &value) const {
            return utils::visit_tagged([this](utils::tag_for<variant_type> auto tag, auto && ... args) {
                return json{{
                    std::string_view{tag.name},
                    serialize_args(std::forward<decltype(args)>(args) ... )
                }};
            }, value);
        }
    };

    template<typename T, typename Context>
    concept void_or_deserializable = std::is_void_v<T> || deserializable<T, Context>;

    template<typename Context, typename ... Ts> requires (void_or_deserializable<typename Ts::type, Context> && ...)
    struct deserializer<utils::tagged_variant<Ts ...>, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;

        using variant_type = utils::tagged_variant<Ts ...>;
        
        variant_type operator()(const json &value) const {
            if (!value.is_object()) {
                throw std::runtime_error("Cannot deserialize tagged variant: value is not an object");
            }
            if (value.size() != 1) {
                throw std::runtime_error("Cannot deserialize tagged variant: object must contain only one key");
            }

            auto key_it = value.begin();
            utils::tagged_variant_index<variant_type> index{std::string_view(key_it.key())};
            const json &inner_value = key_it.value();
            return utils::visit_tagged([&](utils::tag_for<variant_type> auto tag) {
                using value_type = utils::tagged_variant_value_type<variant_type, decltype(tag)>;
                if constexpr (std::is_void_v<value_type>) {
                    return variant_type{tag};
                } else {
                    return variant_type{tag, this->template deserialize_with_context<value_type>(inner_value)};
                }
            }, index);
        }
    };
}

#endif