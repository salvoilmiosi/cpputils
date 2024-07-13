#ifndef __TAGGED_VARIANT_H__
#define __TAGGED_VARIANT_H__

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
    }

    template<tstring Name, typename V> struct tagged_variant_index_of;

    template<typename ... Ts>
    struct tagged_variant : detail::build_tagged_variant<Ts ...>::type {
        using base = detail::build_tagged_variant<Ts ...>::type;
        using base::variant;

        template<tstring Name, typename ... Args>
        tagged_variant(tag<Name>, Args && ... args)
            : base(std::in_place_index<tagged_variant_index_of<Name, tagged_variant>::value>, std::forward<Args>(args) ...) {}
    };

    template<tstring Name, typename T, typename ... Ts>
    struct tagged_variant_index_of<Name, tagged_variant<tag<Name, T>, Ts...>> {
        static constexpr size_t value = 0;
    };

    template<tstring Name, typename First, typename ... Rest>
    struct tagged_variant_index_of<Name, tagged_variant<First, Rest ...>> {
        static constexpr size_t value = 1 + tagged_variant_index_of<Name, tagged_variant<Rest ...>>::value;
    };

    template<typename T> struct tagged_variant_tag_names;

    template<typename ... Ts>
    struct tagged_variant_tag_names<tagged_variant<Ts ...>> {
        static constexpr std::array value { std::string_view(Ts::name) ... };
    };

    namespace detail {
        template<typename T> struct is_tagged_variant : std::false_type {};
        template<typename ... Ts> struct is_tagged_variant<tagged_variant<Ts ...>> : std::true_type {};
    }

    template<typename T>
    concept is_tagged_variant = detail::is_tagged_variant<T>::value;

    template<typename T, typename V>
    concept tag_for = requires {
        requires is_tagged_variant<V>;
        tagged_variant_index_of<T::name, V>::value;
    };

    template<typename V, size_t I> struct tagged_variant_tag_at;

    template<typename First, typename ... Rest>
    struct tagged_variant_tag_at<tagged_variant<First, Rest...>, 0> : std::type_identity<First> {};

    template<typename First, typename ... Rest, size_t I>
    struct tagged_variant_tag_at<tagged_variant<First, Rest...>, I> : tagged_variant_tag_at<tagged_variant<Rest...>, I - 1> {};

    template<typename Visitor, typename ... Ts>
    decltype(auto) visit_tagged(Visitor &&visitor, const tagged_variant<Ts ...> &variant) {
        using variant_type = tagged_variant<Ts ...>;
        static constexpr auto vtable = []<size_t ... Is>(std::index_sequence<Is ...>) {
            return std::array{
                +[](Visitor &&fn, const variant_type &variant) -> decltype(auto) {
                    static constexpr size_t I = Is;
                    using tag_type = typename tagged_variant_tag_at<variant_type, I>::type;
                    if constexpr (std::is_void_v<typename tag_type::type>) {
                        return std::invoke(std::forward<Visitor>(fn), tag<tag_type::name>{});
                    } else {
                        return std::invoke(std::forward<Visitor>(fn), tag<tag_type::name>{}, std::get<I>(variant));
                    }
                } ...
            };
        }(std::index_sequence_for<Ts ...>());
        return vtable[variant.index()](std::forward<Visitor>(visitor), variant);
    }
}

namespace json {
    
    template<typename Context, typename ... Ts>
    struct serializer<utils::tagged_variant<Ts ...>, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;

        using variant_type = utils::tagged_variant<Ts ...>;
        
        json operator()(const variant_type &value) const {
            return utils::visit_tagged([this]<utils::tag_for<variant_type> Tag>(Tag, auto && ... args) -> json {
                std::string key{std::string_view(Tag::name)};
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
            const std::string &key = key_it.key();
            const auto &tag_names = utils::tagged_variant_tag_names<variant_type>::value;
            auto it = rn::find(tag_names, std::string_view(key));
            if (it == tag_names.end()) {
                throw std::runtime_error(fmt::format("Invalid variant type: {}", key));
            }

            static constexpr auto vtable = []<size_t ... Is>(std::index_sequence<Is ...>) {
                return std::array {
                    +[](const deserializer &self, const json &inner_value) -> variant_type {
                        static constexpr size_t I = Is;
                        using tag_type = typename utils::tagged_variant_tag_at<variant_type, I>::type;
                        using value_type = typename tag_type::type;
                        if constexpr (std::is_void_v<value_type>) {
                            return variant_type{std::in_place_index<I>};
                        } else {
                            return variant_type{std::in_place_index<I>, self.template deserialize_with_context<value_type>(inner_value)};
                        }
                    } ...
                };
            }(std::index_sequence_for<Ts ...>());

            return vtable[rn::distance(tag_names.begin(), it)](*this, key_it.value());
        }
    };
}

#endif