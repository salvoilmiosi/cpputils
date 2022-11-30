#ifndef __JSON_SERIAL_H__
#define __JSON_SERIAL_H__

#include "enum_variant.h"
#include "reflector.h"
#include "base64.h"

#include <fmt/format.h>
#include <json/json.h>
#include <vector>
#include <string>
#include <chrono>
#include <map>

namespace json {

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
        auto deserialize_with_context(const Json::Value &value) const {
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
        auto deserialize_with_context(const Json::Value &value) const {
            return deserializer<T, void>{}(value);
        }
    };

    template<typename T> requires serializable<T>
    Json::Value serialize(const T &value) {
        return serializer<T, void>{}(value);
    }

    template<typename T, typename Context> requires serializable<T, Context>
    Json::Value serialize(const T &value, const Context &context) {
        return context_holder<Context>{context}.serialize_with_context(value);
    }

    template<typename T> requires deserializable<T>
    T deserialize(const Json::Value &value) {
        return deserializer<T, void>{}(value);
    }

    template<typename T, typename Context> requires deserializable<T, Context>
    T deserialize(const Json::Value &value, const Context &context) {
        return context_holder<Context>{context}.template deserialize_with_context<T>(value);
    }

    template<typename Context>
    struct serializer<Json::Value, Context> {
        Json::Value operator()(const Json::Value &value) const {
            return value;
        }
    };

    template<std::convertible_to<Json::Value> T, typename Context>
    struct serializer<T, Context> {
        Json::Value operator()(const T &value) const {
            return Json::Value(value);
        }
    };

    template<reflector::reflectable T, typename Context>
    struct serializer<T, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;
        
        template<size_t I>
        static void serialize_field(const serializer &self, const T &value, Json::Value &ret) {
            const auto field_data = reflector::get_field_data<I>(value);
            const auto &field = field_data.get();
            ret[field_data.name()] = self.serialize_with_context(field);
        }

        template<size_t ... Is>
        static void serialize_fields(const serializer &self, const T &value, Json::Value &ret, std::index_sequence<Is...>) {
            (serialize_field<Is>(self, value, ret), ...);
        }

        Json::Value operator()(const T &value) const {
            Json::Value ret;
            serialize_fields(*this, value, ret, std::make_index_sequence<reflector::num_fields<T>>());
            return ret;
        }
    };

    template<enums::enum_with_names T, typename Context>
    struct serializer<T, Context> {
        Json::Value operator()(const T &value) const {
            return std::string(enums::to_string(value));
        }
    };

    template<enums::enum_with_names T, typename Context> requires enums::flags_enum<T>
    struct serializer<T, Context> {
        Json::Value operator()(const T &value) const {
            using namespace enums::flag_operators;
            Json::Value ret = Json::arrayValue;
            for (T v : enums::enum_values_v<T>) {
                if (bool(value & v)) {
                    ret.append(Json::String(enums::value_to_string(v)));
                }
            }
            return ret;
        }
    };

    template<serializable T, typename Context>
    struct serializer<std::vector<T>, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;
        
        Json::Value operator()(const std::vector<T> &value) const {
            Json::Value ret = Json::arrayValue;
            for (const T &obj : value) {
                ret.append(this->serialize_with_context(obj));
            }
            return ret;
        }
    };

    template<typename Context>
    struct serializer<std::vector<std::byte>, Context> {
        Json::Value operator()(const std::vector<std::byte> &value) const {
            return base64::base64_encode(value);
        }
    };

    template<serializable T, typename Context>
    struct serializer<std::map<std::string, T>, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;
        
        Json::Value operator()(const std::map<std::string, T> &value) const {
            Json::Value ret = Json::objectValue;
            for (const auto &[key, value] : value) {
                ret[key] = this->serialize_with_context(value);
            }
            return ret;
        }
    };

    template<enums::is_enum_variant T, typename Context>
    struct serializer<T, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;
        
        Json::Value operator()(const T &value) const {
            using enum_type = typename T::enum_type;
            return enums::visit_indexed([this]<enum_type E>(enums::enum_tag_t<E>, auto && ... args) {
                Json::Value ret = Json::objectValue;
                ret["type"] = serializer<enum_type>{}(E);
                if constexpr (sizeof...(args) > 0) {
                    ret["value"] = this->serialize_with_context(FWD(args) ... );
                }
                return ret;
            }, value);
        }
    };

    template<serializable ... Ts, typename Context>
    struct serializer<std::variant<Ts ...>, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;
        
        Json::Value operator()(const std::variant<Ts ...> &value) const {
            Json::Value ret = Json::objectValue;
            ret["index"] = value.index();
            ret["value"] = std::visit([this](const auto &value) {
                return this->serialize_with_context(value);
            }, value);
            return ret;
        }
    };

    template<typename Rep, typename Period, typename Context>
    struct serializer<std::chrono::duration<Rep, Period>, Context> {
        Json::Value operator()(const std::chrono::duration<Rep, Period> &value) const {
            return value.count();
        }
    };
    
    template<typename Context>
    struct deserializer<Json::Value, Context> {
        Json::Value operator()(const Json::Value &value) const {
            return value;
        }
    };

    template<typename T>
    concept convertible_from_json_value = requires (const Json::Value &value) {
        { value.as<T>() } -> std::convertible_to<T>;
    };

    template<convertible_from_json_value T, typename Context>
    struct deserializer<T, Context> {
        T operator()(const Json::Value &value) const {
            return value.as<T>();
        }
    };

    template<std::integral T, typename Context> requires (!convertible_from_json_value<T>)
    struct deserializer<T, Context> {
        T operator()(const Json::Value &value) const {
            return value.asInt();
        }
    };

    template<reflector::reflectable T, typename Context> requires std::is_default_constructible_v<T>
    struct deserializer<T, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;
        
        template<size_t I>
        static void deserialize_field(const deserializer &self, const Json::Value &value, T &out) {
            auto field_data = reflector::get_field_data<I>(out);
            auto &field = field_data.get();
            if (value.isMember(field_data.name())) {
                field = self.template deserialize_with_context<std::remove_cvref_t<decltype(field)>>(value[field_data.name()]);
            }
        }

        template<size_t ... Is>
        static void deserialize_fields(const deserializer &self, const Json::Value &value, T &out, std::index_sequence<Is ...>) {
            (deserialize_field<Is>(self, value, out), ...);
        }

        T operator()(const Json::Value &value) const {
            T ret;
            deserialize_fields(*this, value, ret, std::make_index_sequence<reflector::num_fields<T>>());
            return ret;
        }
    };

    template<enums::enum_with_names T, typename Context>
    struct deserializer<T, Context> {
        T operator()(const Json::Value &value) const {
            if (value.isString()) {
                std::string str = value.asString();
                if (auto ret = enums::from_string<T>(str)) {
                    return *ret;
                } else {
                    throw Json::RuntimeError(fmt::format("Invalid {}: {}", enums::enum_name_v<T>, str));
                }
            } else {
                throw Json::RuntimeError("Value is not a string");
            }
        }
    };

    template<enums::enum_with_names T, typename Context> requires enums::flags_enum<T>
    struct deserializer<T, Context> {
        T operator()(const Json::Value &value) const {
            using namespace enums::flag_operators;
            if (value.isArray()) {
                T ret{};
                for (const auto &elem : value) {
                    if (elem.isString()) {
                        if (auto v = enums::value_from_string<T>(elem.asString())) {
                            ret |= *v;
                        } else {
                            throw Json::RuntimeError(fmt::format("Invalid {}: {}", enums::enum_name_v<T>, elem.asString()));
                        }
                    } else {
                        throw Json::RuntimeError("Elem is not a string");
                    }
                }
                return ret;
            } else if (value.isString()) {
                if (auto ret = enums::from_string<T>(value.asString())) {
                    return *ret;
                } else {
                    throw Json::RuntimeError(fmt::format("Invalid {}: {}", enums::enum_name_v<T>, value.asString()));
                }
            } else {
                throw Json::RuntimeError("Invalid type for value");
            }
        }
    };

    template<deserializable T, typename Context>
    struct deserializer<std::vector<T>, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;
        
        std::vector<T> operator()(const Json::Value &value) const {
            std::vector<T> ret;
            for (const auto &obj : value) {
                ret.push_back(this->template deserialize_with_context<T>(obj));
            }
            return ret;
        }
    };

    template<typename Context>
    struct deserializer<std::vector<std::byte>, Context> {
        std::vector<std::byte> operator()(const Json::Value &value) const {
            if (value.isString()) {
                return base64::base64_decode(value.asString());
            } else {
                throw Json::RuntimeError("Value is not a string");
            }
        }
    };

    template<deserializable T, typename Context>
    struct deserializer<std::map<std::string, T>, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;
        
        T operator()(const Json::Value &value) const {
            std::map<std::string, T> ret;
            for (auto it=value.begin(); it!=value.end(); ++it) {
                ret.emplace(it.key(), this->template deserialize_with_context<T>(*it));
            }
            return ret;
        }
    };

    template<enums::is_enum_variant T, typename Context>
    struct deserializer<T, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;
        
        T operator()(const Json::Value &value) const {
            if (!value.isMember("type")) {
                throw Json::RuntimeError("Missing field 'type' in enums::enum_variant");
            }
            using enum_type = typename T::enum_type;
            enum_type variant_type = this->template deserialize_with_context<enum_type>(value["type"]);
            return enums::visit_enum([&]<enum_type E>(enums::enum_tag_t<E> tag) {
                if constexpr (T::template has_type<E>) {
                    return T(tag, this->template deserialize_with_context<typename T::template value_type<E>>(value["value"]));
                } else {
                    return T(tag);
                }
            }, variant_type);
        }
    };

    template<deserializable ... Ts, typename Context>
    struct deserializer<std::variant<Ts ...>, Context> : context_holder<Context> {
        using context_holder<Context>::context_holder;
        using variant_type = std::variant<Ts ...>;

        template<typename T>
        static variant_type deserialize_alternative(const deserializer &self, const Json::Value &value) {
            return self.template deserialize_with_context<T>(value);
        }

        variant_type operator()(const Json::Value &value) const {
            if (!value.isMember("index")) {
                throw Json::RuntimeError("Missing field 'index' in std::variant");
            }

            static constexpr auto vtable = std::array { deserialize_alternative<Ts> ... };
            return vtable[value["index"].asInt()](*this, value["value"]);
        }
    };

    template<typename Rep, typename Period, typename Context>
    struct deserializer<std::chrono::duration<Rep, Period>, Context> {
        std::chrono::duration<Rep, Period> operator()(const Json::Value &value) const {
            return std::chrono::duration<Rep, Period>{value.as<Rep>()};
        }
    };
}

#endif