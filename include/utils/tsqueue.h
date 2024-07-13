#ifndef __TSQUEUE_H__
#define __TSQUEUE_H__

#include <mutex>
#include <deque>
#include <optional>

#include "type_list.h"

namespace utils {
    template<typename T, size_t MaxSize = std::numeric_limits<size_t>::max()>
    class tsqueue {
    public:
        tsqueue() = default;
        tsqueue(const tsqueue &) = delete;
        tsqueue(tsqueue &&) = default;

        tsqueue &operator = (const tsqueue &) = delete;
        tsqueue &operator = (tsqueue &&) = delete;

    public:
        T &emplace_back(auto && ... args) {
            std::scoped_lock lock(m_mutex);
            T &ret = m_queue.emplace_back(FWD(args) ... );
            if (m_queue.size() > MaxSize) {
                m_queue.pop_front();
            }
            return ret;
        }

        void push_back(const T &value) {
            emplace_back(value);
        }

        void push_back(T &&value) {
            emplace_back(std::move(value));
        }

       std::optional<T> pop_front() {
            std::scoped_lock lock(m_mutex);
            if (m_queue.empty()) {
                return std::nullopt;
            }
            std::optional<T> value = std::move(m_queue.front());
            m_queue.pop_front();
            return value;
        }

        void clear() {
            std::scoped_lock lock(m_mutex);
            m_queue.clear();
        }

    private:
        std::mutex m_mutex;
        std::deque<T> m_queue;
    };
}

#endif