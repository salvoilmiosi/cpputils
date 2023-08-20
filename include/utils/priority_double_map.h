#ifndef __PRIORITY_DOUBLE_MAP_H__
#define __PRIORITY_DOUBLE_MAP_H__

#include <functional>
#include <map>
#include <set>

#include "enums.h"

namespace util {

    template<enums::reflected_enum EnumType, typename Key, typename Compare = std::greater<Key>>
    struct priority_double_map {
    private:
        enum value_status : uint8_t {
            inactive, active, erased
        };

        struct value_variant {
            const enums::enum_variant<EnumType> value;
            value_status status = inactive;

            template<typename ... Ts>
            value_variant(Ts && ... args) : value{FWD(args)...} {}
        };
        
        struct iterator_compare {
            template<typename T>
            bool operator()(const T &lhs, const T &rhs) const {
                return Compare{}(lhs->first, rhs->first)
                    || !Compare{}(rhs->first, lhs->first)
                    && (&*lhs < &*rhs);
            }
        };

        using container_map = std::multimap<Key, value_variant, std::less<>>;
        using container_iterator = typename container_map::iterator;
        using iterator_set = std::set<container_iterator, iterator_compare>;

        struct set_lock_pair {
            uint8_t lock_count = 0;
            iterator_set set;
        };

        template<std::invocable Function, typename T>
        class on_destroy_do : public T {
        private:
            Function m_fun;
        
        public:
            on_destroy_do(Function &&fun, T &&value)
                : T{std::move(value)}
                , m_fun(fun) {}

            on_destroy_do(const on_destroy_do &) = delete;
            on_destroy_do(on_destroy_do &&other)
                noexcept(std::is_nothrow_move_constructible_v<T>
                    && std::is_nothrow_move_constructible_v<Function>)
                : T{std::move(other)}
                , m_fun{std::move(other.m_fun)} {}

            on_destroy_do &operator = (const on_destroy_do &) = delete;
            on_destroy_do &operator = (on_destroy_do &&other)
                noexcept(std::is_nothrow_move_assignable_v<T>
                    && std::is_nothrow_move_assignable_v<Function>)
            {
                static_cast<T &>(*this) = std::move(other);
                std::swap(m_fun, other.m_fun);
                return *this;
            }

            ~on_destroy_do() {
                std::invoke(m_fun);
            }
        };

        using iterator_table = std::array<set_lock_pair, enums::num_members_v<EnumType>>;
        using iterator_vector = std::vector<container_iterator>;

        container_map m_map;
        iterator_table m_table;
        iterator_vector m_changes;

    public:
        template<EnumType E, typename ... Ts>
        void add(Key key, Ts && ... args) {
            auto it = m_map.emplace(std::piecewise_construct,
                std::make_tuple(std::move(key)),
                std::make_tuple(enums::enum_tag<E>, FWD(args) ...));
            m_table[enums::indexof(E)].set.emplace(it);
            m_changes.push_back(it);
        }

        template<typename T>
        void erase(const T &key) {
            auto [low, high] = m_map.equal_range(key);
            for (; low != high; ++low) {
                if (low->second.status != erased) {
                    low->second.status = erased;
                    m_changes.push_back(low);
                }
            }
        }

        void commit_changes() {
            for (auto it = m_changes.begin(); it != m_changes.end();) {
                auto map_it = *it;
                auto &set = m_table[map_it->second.value.index()];
                if (set.lock_count == 0) {
                    switch (map_it->second.status) {
                    case inactive:
                        map_it->second.status = active;
                        break;
                    case erased:
                        set.set.erase(map_it);
                        m_map.erase(map_it);
                    }
                    it = m_changes.erase(it);
                } else {
                    ++it;
                }
            }
        }

        template<EnumType E>
        auto get_table() {
            auto &set = m_table[enums::indexof(E)];
            ++set.lock_count;
            return on_destroy_do([&]{
                --set.lock_count;
            }, set.set
                | std::views::filter([](container_iterator it) {
                    return it->second.status == active;
                })
                | std::views::transform([](container_iterator it) -> decltype(auto) {
                    return it->second.value.template get<E>();
                }));
        }
    };

}

#endif