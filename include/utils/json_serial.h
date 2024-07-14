#ifndef __JSON_SERIAL_H__
#define __JSON_SERIAL_H__

#include "base64.h"
#include "utils.h"

#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <reflect>

#include <vector>
#include <string>
#include <chrono>
#include <map>

namespace json {

    using json = nlohmann::json;

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
        return deserializer<T, void>{}(value);
    }

    template<typename T, typename Context> requires deserializable<T, Context>
    T deserialize(const json &value, const Context &context) {
        return context_holder<Context>{context}.template deserialize_with_context<T>(value);
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
            auto ret = json::object();
            reflect::for_each<T>([&](auto I) {
                ret.push_back(json::object_t::value_type(reflect::member_name<I, T>(),
                    this->template serialize_with_context(reflect::get<I>(value))));
            });
            return ret;
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

    template<typename Context>
    struct serializer<base64::encoded_bytes, Context> {
        json operator()(const base64::encoded_bytes &value) const {
            return value.to_string();
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
            return value.get<T>();
        }
    };

    template<typename Context>
    struct deserializer<std::string, Context> {
        std::string operator()(const json &value) const {
            return value.get<std::string>();
        }
    };

    template<typename T, typename Context> requires (std::is_aggregate_v<T> && std::is_default_constructible_v<T>)
    struct deserializer<T, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;

        T operator()(const json &value) const {
            T ret{};
            reflect::for_each<T>([&](auto I) {
                auto name = reflect::member_name<I, T>();
                if (value.contains(name)) {
                    auto &field = reflect::get<I>(ret);
                    using value_type = std::remove_cvref_t<decltype(field)>;
                    field = this->template deserialize_with_context<value_type>(value[name]);
                }
            });
            return ret;
        }
    };

    template<deserializable T, typename Context>
    struct deserializer<std::vector<T>, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;
        
        std::vector<T> operator()(const json &value) const {
            std::vector<T> ret;
            ret.reserve(value.size());
            for (const auto &obj : value) {
                ret.push_back(this->template deserialize_with_context<T>(obj));
            }
            return ret;
        }
    };

    template<typename Context>
    struct deserializer<base64::encoded_bytes, Context> {
        base64::encoded_bytes operator()(const json &value) const {
            if (value.is_string()) {
                return base64::encoded_bytes(value.get<std::string>());
            } else {
                throw std::runtime_error("Value is not a string");
            }
        }
    };

    template<typename Rep, typename Period, typename Context>
    struct deserializer<std::chrono::duration<Rep, Period>, Context> {
        std::chrono::duration<Rep, Period> operator()(const json &value) const {
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