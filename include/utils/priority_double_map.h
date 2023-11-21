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

        using iterator_table = std::array<set_lock_pair, enums::num_members_v<EnumType>>;
        using iterator_vector = std::vector<container_iterator>;

        container_map m_map;
        iterator_table m_table;
        iterator_vector m_changes;

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

    public:
        template<EnumType E, typename ... Ts>
        void add(Key key, Ts && ... args) {
            auto it = m_map.emplace(std::piecewise_construct,
                std::make_tuple(std::move(key)),
                std::make_tuple(enums::enum_tag<E>, FWD(args) ...));
            m_table[enums::indexof(E)].set.emplace(it);
            m_changes.push_back(it);
            commit_changes();
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
            commit_changes();
        }

        template<EnumType E>
        class table_lock {
        private:
            priority_double_map *parent;

            set_lock_pair &get_table() const {
                return parent->m_table[enums::indexof(E)];
            }

        public:
            table_lock(priority_double_map *parent) : parent{parent} {
                ++get_table().lock_count;
            }

            table_lock(const table_lock &other) = delete;
            table_lock(table_lock &&other) noexcept : parent{std::exchange(other.parent, nullptr)} {}

            table_lock &operator = (const table_lock &other) = delete;
            table_lock &operator = (table_lock &&other) noexcept { std::swap(parent, other.parent); return *this; }

            ~table_lock() {
                if (parent) {
                    --get_table().lock_count;
                    parent->commit_changes();
                }
            }

            auto values() const {
                return get_table().set 
                    | std::views::filter([](container_iterator it) {
                        return it->second.status == active;
                    })
                    | std::views::transform([](container_iterator it) -> decltype(auto) {
                        return it->second.value.template get<E>();
                    });
            }
        };

        template<EnumType E>
        auto lock_table() {
            return table_lock<E>(this);
        }
    };

}

#endif