#ifndef __JSON_SERIAL_H__
#define __JSON_SERIAL_H__

#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <reflect>

#include <vector>
#include <string>
#include <chrono>
#include <map>

namespace json {

    using json = nlohmann::ordered_json;

    template<typename T, typename Context = void> struct serializer;

    template<typename T, typename Context = void>
    concept serializable = requires {
        typename serializer<T, Context>;
    };

    template<typename T, typename Context = void> struct deserializer;

    template<typename T, typename Context = void>
    concept deserializable = requires {
        typename deserializer<T, Context>;
    };

    using json_error = json::exception;

    struct deserialize_error : json_error {
        deserialize_error(const char *message): json_error(0, message) {}
    };

    template<typename Context>
    struct context_holder {
        const Context &context;

        context_holder(const Context &context) : context(context) {}

        template<serializable<Context> T>
        auto serialize_with_context(const T &value) const {
            if constexpr (requires { serializer<T, Context>{context}; }) {
                return serializer<T, Context>{context}(value);
            } else {
                return serializer<T, void>{}(value);
            }
        }

        template<deserializable<Context> T>
        auto deserialize_with_context(const json &value) const {
            if constexpr (requires { deserializer<T, Context>{context}; }) {
                return deserializer<T, Context>{context}(value);
            } else {
                return deserializer<T, void>{}(value);
            }
        }
    };

    template<> struct context_holder<void> {
        template<serializable T>
        auto serialize_with_context(const T &value) const {
            return serializer<T, void>{}(value);
        }

        template<deserializable T>
        auto deserialize_with_context(const json &value) const {
            return deserializer<T, void>{}(value);
        }
    };

    template<typename T> requires serializable<T>
    json serialize(const T &value) {
        return serializer<T, void>{}(value);
    }

    template<typename T, typename Context> requires serializable<T, Context>
    json serialize(const T &value, const Context &context) {
        return context_holder<Context>{context}.serialize_with_context(value);
    }

    template<typename T> requires deserializable<T>
    T deserialize(const json &value) {
        try {
            return deserializer<T, void>{}(value);
        } catch (const std::exception &e) {
            throw deserialize_error(e.what());
        }
    }

    template<typename T, typename Context> requires deserializable<T, Context>
    T deserialize(const json &value, const Context &context) {
        try {
            return context_holder<Context>{context}.template deserialize_with_context<T>(value);
        } catch (const std::exception &e) {
            throw deserialize_error(e.what());
        }
    }

    template<typename Context>
    struct serializer<json, Context> {
        json operator()(const json &value) const {
            return value;
        }
    };

    template<typename T, typename Context> requires std::is_arithmetic_v<T>
    struct serializer<T, Context> {
        json operator()(const T &value) const {
            return value;
        }
    };

    template<typename Context>
    struct serializer<std::string, Context> {
        json operator()(const std::string &value) const {
            return value;
        }
    };

    template<typename T, typename Context> requires std::is_aggregate_v<T>
    struct serializer<T, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;

        json operator()(const T &value) const {
            return [&]<size_t ... Is>(std::index_sequence<Is ...>) {
                return json{{
                    reflect::member_name<Is, T>(),
                    this->template serialize_with_context(reflect::get<Is>(value))
                } ... };
            }(std::make_index_sequence<reflect::size<T>()>());
        }
    };

    template<serializable T, typename Context>
    struct serializer<std::vector<T>, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;
        
        json operator()(const std::vector<T> &value) const {
            auto ret = json::array();
            ret.get_ptr<json::array_t*>()->reserve(value.size());
            for (const T &obj : value) {
                ret.push_back(this->serialize_with_context(obj));
            }
            return ret;
        }
    };

    template<typename Rep, typename Period, typename Context>
    struct serializer<std::chrono::duration<Rep, Period>, Context> {
        json operator()(const std::chrono::duration<Rep, Period> &value) const {
            return value.count();
        }
    };

    template<typename T, typename Context>
    struct serializer<std::optional<T>, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;

        json operator()(const std::optional<T> &value) const {
            if (value) {
                return this->serialize_with_context(*value);
            } else {
                return json{};
            }
        }
    };
    
    template<typename Context>
    struct deserializer<json, Context> {
        json operator()(const json &value) const {
            return value;
        }
    };

    template<typename T, typename Context> requires std::is_arithmetic_v<T>
    struct deserializer<T, Context> {
        T operator()(const json &value) const {
            if constexpr (std::is_same_v<T, bool>) {
                if (!value.is_boolean()) {
                    throw std::runtime_error("Cannot deserialize boolean");
                }
            } else if constexpr (std::is_integral_v<T>) {
                if (!value.is_number_integer()) {
                    throw std::runtime_error("Cannot deserialize integer");
                }
            } else {
                if (!value.is_number()) {
                    throw std::runtime_error("Cannot deserialize number");
                }
            }
            return value.get<T>();
        }
    };

    template<typename Context>
    struct deserializer<std::string, Context> {
        std::string operator()(const json &value) const {
            if (!value.is_string()) {
                throw std::runtime_error("Cannot deserialize string");
            }
            return value.get<std::string>();
        }
    };

    template<typename T, typename Context> requires std::is_aggregate_v<T>
    struct deserializer<T, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;

        template<size_t I>
        using member_type = std::remove_cvref_t<decltype(reflect::get<I>(std::declval<T>()))>;

        template<size_t I>
        member_type<I> deserialize_field(const json &value) const {
            static constexpr auto name = reflect::member_name<I, T>();
            using value_type = member_type<I>;
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
            return [&]<size_t ... Is>(std::index_sequence<Is ...>) {
                return T{ deserialize_field<Is>(value) ... };
            }(std::make_index_sequence<reflect::size<T>()>());
        }
    };
    

    template<deserializable T, typename Context>
    struct deserializer<std::vector<T>, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;
        
        std::vector<T> operator()(const json &value) const {
            if (!value.is_array()) {
                throw std::runtime_error(fmt::format("Cannot deserialize vector of {}", reflect::type_name<T>()));
            }
            std::vector<T> ret;
            ret.reserve(value.size());
            for (const auto &obj : value) {
                ret.push_back(this->template deserialize_with_context<T>(obj));
            }
            return ret;
        }
    };

    template<typename Rep, typename Period, typename Context>
    struct deserializer<std::chrono::duration<Rep, Period>, Context> {
        std::chrono::duration<Rep, Period> operator()(const json &value) const {
            if (!value.is_number()) {
                throw std::runtime_error("Cannot deserialize duration: value is not a number");
            }
            return std::chrono::duration<Rep, Period>{value.get<Rep>()};
        }
    };

    template<typename T, typename Context>
    struct deserializer<std::optional<T>, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;

        std::optional<T> operator()(const json &value) const {
            if (value.is_null()) {
                return std::nullopt;
            } else {
                return this->template deserialize_with_context<T>(value);
            }
        }
    };
}

#endif