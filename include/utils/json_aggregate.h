#ifndef __JSON_AGGREGATE_H__
#define __JSON_AGGREGATE_H__

#include "json_serial.h"
#include <reflect>

namespace json {

    template<typename T>
    concept aggregate = std::is_aggregate_v<T>;
    
    template<aggregate T, size_t I>
    using member_type = std::remove_cvref_t<decltype(reflect::get<I>(std::declval<T>()))>;

    template<typename T, typename Context>
    concept all_fields_serializable = aggregate<T> && 
        []<size_t ... Is>(std::index_sequence<Is ...>) {
            return (serializable<member_type<T, Is>, Context> && ...);
        }(std::make_index_sequence<reflect::size<T>()>());

    template<typename T, typename Context>
    concept all_fields_deserializable = aggregate<T> &&
        []<size_t ... Is>(std::index_sequence<Is ...>) {
            return (deserializable<member_type<T, Is>, Context> && ...);
        }(std::make_index_sequence<reflect::size<T>()>());

    template<aggregate T, typename Context> requires all_fields_serializable<T, Context>
    struct serializer<T, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;

        json operator()(const T &value) const {
            return [&]<size_t ... Is>(std::index_sequence<Is ...>) {
                return json::object({
                    {
                        reflect::member_name<Is, T>(),
                        this->template serialize_with_context(reflect::get<Is>(value))
                    } ... 
                });
            }(std::make_index_sequence<reflect::size<T>()>());
        }
    };

    template<aggregate T, typename Context> requires all_fields_deserializable<T, Context>
    struct deserializer<T, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;

        template<size_t I>
        member_type<T, I> deserialize_field(const json &value) const {
            static constexpr auto name = reflect::member_name<I, T>();
            using value_type = member_type<T, I>;
            if (value.contains(name)) {
                return this->template deserialize_with_context<value_type>(value[name]);
            } else if constexpr (std::is_default_constructible_v<value_type>) {
                return value_type{};
            } else {
                throw std::runtime_error(fmt::format("missing field {}", name));
            }
        }

        T operator()(const json &value) const {
            if (!value.is_object()) {
                throw std::runtime_error(fmt::format("Cannot deserialize {}: value is not an object", reflect::type_name<T>()));
            }
            if constexpr (requires { typename T::keep_default_values; }) {
                T result{};
                reflect::for_each<T>([&](auto I) {
                    static constexpr auto name = reflect::member_name<I, T>();
                    using value_type = member_type<T, I>;
                    if (value.contains(name)) {
                        reflect::get<I>(result) = this->template deserialize_with_context<value_type>(value[name]);
                    }
                });
                return result;
            } else {
                return [&]<size_t ... Is>(std::index_sequence<Is ...>) {
                    return T{ deserialize_field<Is>(value) ... };
                }(std::make_index_sequence<reflect::size<T>()>());
            }
        }
    };

}

#endif