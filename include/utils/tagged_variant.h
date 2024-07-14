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

    namespace detail {
        template<typename T> using type_or_monostate = std::conditional_t<std::is_void_v<T>, std::monostate, T>;

        template<typename ... Ts>
        struct build_tagged_variant {
            using type = std::variant<type_or_monostate<typename Ts::type> ...>;
        };

        template<typename Variant, tstring Name> struct find_tag_name;
    }

    template<typename ... Ts>
    struct tagged_variant : detail::build_tagged_variant<Ts ...>::type {
        using base = detail::build_tagged_variant<Ts ...>::type;
        using base::variant;

        template<tstring Name, typename ... Args>
        tagged_variant(tag<Name>, Args && ... args)
            : base(std::in_place_index<detail::find_tag_name<tagged_variant, Name>::index>, std::forward<Args>(args) ...) {}
    };

    namespace detail {
        template<tstring Name, typename T, typename ... Ts>
        struct find_tag_name<tagged_variant<tag<Name, T>, Ts...>, Name> {
            static constexpr auto name = Name;
            static constexpr size_t index = 0;
            using value_type = T;
        };

        template<tstring Name, typename First, typename ... Rest>
        struct find_tag_name<tagged_variant<First, Rest ...>, Name> {
            using next = find_tag_name<tagged_variant<Rest ...>, Name>;

            static constexpr auto name = next::name;
            static constexpr size_t index = 1 + next::index;
            using value_type = typename next::value_type;
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
        requires is_tagged_variant<Variant>;
        detail::find_tag_name<Variant, Tag::name>::index;
    };

    template<typename Variant, tstring Name> requires tag_for<tag<Name>, Variant>
    using tagged_variant_value_type = typename detail::find_tag_name<Variant, Name>::value_type;

    template<tstring Name, typename Variant> requires tag_for<tag<Name>, std::remove_cvref_t<Variant>>
    decltype(auto) get(Variant &&variant) {
        return std::get<detail::find_tag_name<std::remove_cvref_t<Variant>, Name>::index>(variant);
    }
    
    template<typename Variant>
    class tagged_variant_index {
    private:
        size_t m_index;

    public:
        constexpr tagged_variant_index() = default;

        constexpr tagged_variant_index(const Variant &variant)
            : m_index{variant.index()} {}

        constexpr tagged_variant_index(tag_for<Variant> auto tag)
            : m_index{detail::find_tag_name<Variant, tag.name>::index} {}
        
        constexpr tagged_variant_index(std::string_view key) {
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

    template<typename Visitor, typename First, typename ... Ts>
    struct visit_return_type<Visitor, tagged_variant<First, Ts ...>> : std::invoke_result<Visitor, tag<First::name>> {};

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
        using return_type = typename visit_return_type<Visitor, tagged_variant<Ts ...>>::type;
        return visit_tagged<return_type>(std::forward<Visitor>(visitor), index);
    }

    template<typename RetType, typename Visitor, typename Variant> requires is_tagged_variant<std::remove_cvref_t<Variant>>
    RetType visit_tagged(Visitor &&visitor, Variant &&variant) {
        using variant_type = std::remove_cvref_t<Variant>;
        return visit_tagged<RetType>([&](tag_for<variant_type> auto tag) -> RetType {
            if constexpr (std::is_void_v<tagged_variant_value_type<variant_type, tag.name>>) {
                return std::invoke(std::forward<Visitor>(visitor), tag);
            } else {
                return std::invoke(std::forward<Visitor>(visitor), tag, get<tag.name>(forward<Variant>(variant)));
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
            return value_type{std::string_view(value.get<std::string>())};
        }
    };
    
    template<typename Context, typename ... Ts>
    struct serializer<utils::tagged_variant<Ts ...>, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;

        using variant_type = utils::tagged_variant<Ts ...>;
        
        json operator()(const variant_type &value) const {
            return utils::visit_tagged([this](utils::tag_for<variant_type> auto tag, auto && ... args) -> json {
                std::string key{std::string_view(tag.name)};
                if constexpr (sizeof...(args) > 0) {
                    return json::object({
                        {key, this->serialize_with_context(FWD(args) ... )}
                    });
                } else {
                    return json::object({
                        {key, json::object()}
                    });
                }
            }, value);
        }
    };

    template<typename Context, typename ... Ts>
    struct deserializer<utils::tagged_variant<Ts ...>, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;

        using variant_type = utils::tagged_variant<Ts ...>;
        
        variant_type operator()(const json &value) const {
            if (value.size() != 1) {
                throw std::runtime_error("Missing type key in utils::tagged_variant");
            }

            auto key_it = value.begin();
            utils::tagged_variant_index<variant_type> index{std::string_view(key_it.key())};
            const json &inner_value = key_it.value();
            return utils::visit_tagged([&](utils::tag_for<variant_type> auto tag) {
                using value_type = utils::tagged_variant_value_type<variant_type, tag.name>;
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