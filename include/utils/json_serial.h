#ifndef __JSON_SERIAL_H__
#define __JSON_SERIAL_H__

#include "enum_variant.h"
#include "reflector.h"
#include "base64.h"

#include <nlohmann/json.hpp>

#include <fmt/format.h>
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

    template<reflector::reflectable T, typename Context>
    struct serializer<T, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;
        
        template<size_t I>
        static void serialize_field(const serializer &self, const T &value, json &ret) {
            const auto field_data = reflector::get_field_data<I>(value);
            const auto &field = field_data.get();
            json json_value = self.serialize_with_context(field);
#ifdef JSON_REMOVE_EMPTY_OBJECTS
            if (json_value.empty()) return;
#endif
            ret.push_back(json::object_t::value_type(field_data.name(), std::move(json_value)));
        }

        template<size_t ... Is>
        static void serialize_fields(const serializer &self, const T &value, json &ret, std::index_sequence<Is...>) {
            (serialize_field<Is>(self, value, ret), ...);
        }

        json operator()(const T &value) const {
            auto ret = json::object();
            serialize_fields(*this, value, ret, std::make_index_sequence<reflector::num_fields<T>>());
            return ret;
        }
    };

    template<enums::enum_with_names T, typename Context>
    struct serializer<T, Context> {
        json operator()(const T &value) const {
            return std::string(enums::to_string(value));
        }
    };

#ifndef JSON_FLATTEN_ENUM_FLAGS
    template<enums::enum_with_names T, typename Context> requires enums::flags_enum<T>
    struct serializer<T, Context> {
        json operator()(const T &value) const {
            using namespace enums::flag_operators;
            auto ret = json::array();
            for (T v : enums::enum_values_v<T>) {
                if (bool(value & v)) {
                    ret.push_back(enums::value_to_string(v));
                }
            }
            return ret;
        }
    };
#endif

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

    template<enums::is_enum_variant T, typename Context>
    struct serializer<T, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;
        
        json operator()(const T &value) const {
            using enum_type = typename T::enum_type;
            return enums::visit_indexed([this]<enum_type E>(enums::enum_tag_t<E>, auto && ... args) -> json {
                std::string key{enums::to_string(E)};
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

    template<serializable ... Ts, typename Context>
    struct serializer<std::variant<Ts ...>, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;
        
        json operator()(const std::variant<Ts ...> &value) const {
            auto ret = json::object();
            ret.push_back(json::object_t::value_type("index", value.index()));
            ret.push_back(json::object_t::value_type("value", std::visit([this](const auto &value) {
                return this->serialize_with_context(value);
            }, value)));
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
            return value.get<T>();
        }
    };

    template<typename Context>
    struct deserializer<std::string, Context> {
        std::string operator()(const json &value) const {
            return value.get<std::string>();
        }
    };

    template<reflector::reflectable T, typename Context> requires std::is_default_constructible_v<T>
    struct deserializer<T, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;
        
        template<size_t I>
        static void deserialize_field(const deserializer &self, const json &value, T &out) {
            auto field_data = reflector::get_field_data<I>(out);
            auto &field = field_data.get();
            if (value.contains(field_data.name())) {
                field = self.template deserialize_with_context<std::remove_cvref_t<decltype(field)>>(value[field_data.name()]);
            }
        }

        template<size_t ... Is>
        static void deserialize_fields(const deserializer &self, const json &value, T &out, std::index_sequence<Is ...>) {
            (deserialize_field<Is>(self, value, out), ...);
        }

        T operator()(const json &value) const {
            T ret;
            deserialize_fields(*this, value, ret, std::make_index_sequence<reflector::num_fields<T>>());
            return ret;
        }
    };

    template<enums::enum_with_names T, typename Context>
    struct deserializer<T, Context> {
        T operator()(const json &value) const {
            if (value.is_string()) {
                auto str = value.get<std::string>();
                if (auto ret = enums::from_string<T>(str)) {
                    return *ret;
                } else {
                    throw std::runtime_error(fmt::format("Invalid {}: {}", enums::enum_name_v<T>, str));
                }
            } else {
                throw std::runtime_error("Value is not a string");
            }
        }
    };

    template<enums::enum_with_names T, typename Context> requires enums::flags_enum<T>
    struct deserializer<T, Context> {
        T operator()(const json &value) const {
            using namespace enums::flag_operators;
            if (value.is_array()) {
                T ret{};
                for (const auto &elem : value) {
                    if (elem.is_string()) {
                        if (auto v = enums::value_from_string<T>(elem.get<std::string>())) {
                            ret |= *v;
                        } else {
                            throw std::runtime_error(fmt::format("Invalid {}: {}", enums::enum_name_v<T>, elem.get<std::string>()));
                        }
                    } else {
                        throw std::runtime_error("Elem is not a string");
                    }
                }
                return ret;
            } else if (value.is_string()) {
                auto str = value.get<std::string>();
                if (auto ret = enums::from_string<T>(str)) {
                    return *ret;
                } else {
                    throw std::runtime_error(fmt::format("Invalid {}: {}", enums::enum_name_v<T>, str));
                }
            } else {
                throw std::runtime_error("Invalid type for value");
            }
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

    template<enums::is_enum_variant T, typename Context>
    struct deserializer<T, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;

        using enum_type = typename T::enum_type;
        
        T operator()(const json &value) const {
            if (value.size() != 1) {
                throw std::runtime_error("Missing type key in enums::enum_variant");
            }
            auto it = value.begin();
            if (auto variant_type = enums::from_string<enum_type>(it.key())) {
                const json &inner_value = it.value();
                return enums::visit_enum([&]<enum_type E>(enums::enum_tag_t<E> tag) {
                    if constexpr (T::template has_type<E>) {
#ifdef JSON_REMOVE_EMPTY_OBJECTS
                        if (inner_value.empty()) return T(tag);
#endif
                        return T(tag, this->template deserialize_with_context<typename T::template value_type<E>>(inner_value));
                    }
                    return T(tag);
                }, *variant_type);
            } else {
                throw std::runtime_error(fmt::format("Invalid variant type: {}", it.key()));
            }
        }
    };

    template<deserializable ... Ts, typename Context>
    struct deserializer<std::variant<Ts ...>, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;
        using variant_type = std::variant<Ts ...>;

        template<typename T>
        static variant_type deserialize_alternative(const deserializer &self, const json &value) {
            return self.template deserialize_with_context<T>(value);
        }

        variant_type operator()(const json &value) const {
            if (!value.contains("index")) {
                throw std::runtime_error("Missing field 'index' in std::variant");
            }

            static constexpr auto vtable = std::array { deserialize_alternative<Ts> ... };
            auto fun = vtable[value["index"].get<int>()];
            if (value.contains("value")) {
                return fun(*this, value["value"]);
            } else {
                return fun(*this, json{});
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