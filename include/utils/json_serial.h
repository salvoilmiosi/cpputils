#ifndef __JSON_SERIAL_H__
#define __JSON_SERIAL_H__

#include "enum_variant.h"
#include "reflector.h"
#include "base64.h"

#include <fmt/format.h>
#include <json/json.h>
#include <vector>
#include <string>
#include <map>

namespace json {

    template<typename T> struct serializer {};

    template<typename T>
    concept serializable = requires(T value) {
        serializer<T>{}(value);
    };

    template<std::convertible_to<Json::Value> T>
    struct serializer<T> {
        Json::Value operator()(const T &value) const {
            return Json::Value(value);
        }
    };

    template<reflector::reflectable T>
    struct serializer<T> {
        Json::Value operator()(const T &value) const {
            Json::Value ret;
            [&]<size_t ... I>(std::index_sequence<I ...>) {
                ([&]{
                    const auto field_data = reflector::get_field_data<I>(value);
                    const auto &field = field_data.get();
                    ret[field_data.name()] = serializer<std::remove_cvref_t<decltype(field)>>{}(field);
                }(), ...);
            }(std::make_index_sequence<reflector::num_fields<T>>());
            return ret;
        }
    };

    template<enums::enum_with_names T>
    struct serializer<T> {
        Json::Value operator()(const T &value) const {
            return std::string(enums::to_string(value));
        }
    };

    template<enums::enum_with_names T> requires enums::flags_enum<T>
    struct serializer<T> {
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

    template<serializable T>
    struct serializer<std::vector<T>> {
        Json::Value operator()(const std::vector<T> &value) const {
            Json::Value ret = Json::arrayValue;
            for (const T &obj : value) {
                ret.append(serializer<T>{}(obj));
            }
            return ret;
        }
    };

    template<>
    struct serializer<std::vector<std::byte>> {
        Json::Value operator()(const std::vector<std::byte> &value) const {
            return base64::base64_encode(value);
        }
    };

    template<serializable T>
    struct serializer<std::map<std::string, T>> {
        Json::Value operator()(const std::map<std::string, T> &value) const {
            Json::Value ret = Json::objectValue;
            for (const auto &[key, value] : value) {
                ret[key] = serializer<T>{}(value);
            }
            return ret;
        }
    };

    template<enums::is_enum_variant T>
    struct serializer<T> {
        Json::Value operator()(const T &value) const {
            using enum_type = typename T::enum_type;
            return enums::visit_indexed([]<enum_type E>(enums::enum_tag_t<E>, auto && ... args) {
                Json::Value ret = Json::objectValue;
                ret["type"] = serializer<enum_type>{}(E);
                if constexpr (sizeof...(args) > 0) {
                    ret["value"] = serializer<typename T::value_type<E>>{}(FWD(args) ... );
                }
                return ret;
            }, value);
        }
    };

    template<serializable ... Ts>
    struct serializer<std::variant<Ts ...>> {
        Json::Value operator()(const std::variant<Ts ...> &value) const {
            Json::Value ret = Json::objectValue;
            ret["index"] = value.index();
            ret["value"] = std::visit([](const auto &value) {
                return serializer<std::remove_cvref_t<decltype(value)>>{}(value);
            }, value);
            return ret;
        }
    };

    template<serializable T>
    Json::Value serialize(const T &value) {
        return serializer<T>{}(value);
    }

    template<typename T> struct deserializer {};

    template<typename T>
    concept deserializable = requires(Json::Value value) {
        deserializer<T>{}(value);
    };

    template<typename T>
    concept convertible_from_json_value = requires (const Json::Value &value) {
        { value.as<T>() } -> std::convertible_to<T>;
    };

    template<convertible_from_json_value T> struct deserializer<T> {
        T operator()(const Json::Value &value) const {
            return value.as<T>();
        }
    };

    template<std::integral T> requires (!convertible_from_json_value<T>)
    struct deserializer<T> {
        T operator()(const Json::Value &value) const {
            return value.asInt();
        }
    };

    template<reflector::reflectable T> requires std::is_default_constructible_v<T>
    struct deserializer<T> {
        T operator()(const Json::Value &value) const {
            T ret;
            [&]<size_t ... I>(std::index_sequence<I ...>) {
                ([&]{
                    auto field_data = reflector::get_field_data<I>(ret);
                    auto &field = field_data.get();
                    if (value.isMember(field_data.name())) {
                        field = deserializer<std::remove_cvref_t<decltype(field)>>{}(value[field_data.name()]);
                    }
                }(), ...);
            }(std::make_index_sequence<reflector::num_fields<T>>());
            return ret;
        }
    };

    template<enums::enum_with_names T>
    struct deserializer<T> {
        T operator()(const Json::Value &value) const {
            if (auto ret = enums::from_string<T>(value.asString())) {
                return *ret;
            } else {
                throw Json::RuntimeError(fmt::format("Invalid {}: {}", enums::enum_name_v<T>, value.asString()));
            }
        }
    };

    template<enums::enum_with_names T> requires enums::flags_enum<T>
    struct deserializer<T> {
        T operator()(const Json::Value &value) const {
            using namespace enums::flag_operators;
            if (value.isArray()) {
                T ret{};
                for (const auto &elem : value) {
                    if (auto v = enums::value_from_string<T>(elem.asString())) {
                        ret |= *v;
                    } else {
                        throw Json::RuntimeError(fmt::format("Invalid {}: {}", enums::enum_name_v<T>, elem.asString()));
                    }
                }
                return ret;
            } else if (auto ret = enums::from_string<T>(value.asString())) {
                return *ret;
            } else {
                throw Json::RuntimeError(fmt::format("Invalid {}: {}", enums::enum_name_v<T>, value.asString()));
            }
        }
    };

    template<deserializable T>
    struct deserializer<std::vector<T>> {
        std::vector<T> operator()(const Json::Value &value) const {
            std::vector<T> ret;
            for (const auto &obj : value) {
                ret.push_back(deserializer<T>{}(obj));
            }
            return ret;
        }
    };

    template<>
    struct deserializer<std::vector<std::byte>> {
        std::vector<std::byte> operator()(const Json::Value &value) const {
            return base64::base64_decode(value.asString());
        }
    };

    template<deserializable T>
    struct deserializer<std::map<std::string, T>> {
        T operator()(const Json::Value &value) const {
            std::map<std::string, T> ret;
            for (auto it=value.begin(); it!=value.end(); ++it) {
                ret.emplace(it.key(), deserializer<T>{}(*it));
            }
            return ret;
        }
    };

    template<enums::is_enum_variant T>
    struct deserializer<T> {
        T operator()(const Json::Value &value) const {
            if (!value.isMember("type")) {
                throw Json::RuntimeError("Campo type mancante in enums::enum_variant");
            }
            using enum_type = typename T::enum_type;
            return enums::visit_enum([&]<enum_type E>(enums::enum_tag_t<E> tag) {
                if constexpr (T::template has_type<E>) {
                    return T(tag, deserializer<typename T::value_type<E>>{}(value["value"]));
                } else {
                    return T(tag);
                }
            }, deserializer<enum_type>{}(value["type"]));
        }
    };

    template<deserializable ... Ts>
    struct deserializer<std::variant<Ts ...>> {
        using variant_type = std::variant<Ts ...>;
        variant_type operator()(const Json::Value &value) const {
            if (!value.isMember("index")) {
                throw Json::RuntimeError("Campo index mancante in std::variant");
            }
            static constexpr auto lut = []<size_t ... Is>(std::index_sequence<Is...>){
                return std::array{ +[](const Json::Value &value) -> variant_type {
                    return deserializer<std::variant_alternative_t<Is, variant_type>>{}(value);
                } ... };
            }(std::make_index_sequence<sizeof...(Ts)>());
            return lut[value["index"].asInt()](value["value"]);
        }
    };

    template<deserializable T>
    T deserialize(const Json::Value &value) {
        return deserializer<T>{}(value);
    }
}

#endif