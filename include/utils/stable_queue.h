#ifndef __STABLE_QUEUE_H__
#define __STABLE_QUEUE_H__

#include <queue>

namespace utils {

    template<typename T>
    using stable_element = std::pair<T, size_t>;

    template<typename T, typename Compare>
    struct stable_compare : private Compare {
        stable_compare() requires std::is_default_constructible_v<Compare> = default;

        template<std::convertible_to<Compare> U>
        stable_compare(U &&value) : Compare(std::forward<U>(value)) {}

        bool operator()(const stable_element<T> &lhs, const stable_element<T> &rhs) const {
            return Compare::operator()(lhs.first, rhs.first)
                || !Compare::operator()(rhs.first, lhs.first)
                && rhs.second < lhs.second;
        }
    };

    template <typename T, typename Compare = std::less<T>>
    class stable_priority_queue
        : public std::priority_queue<stable_element<T>, std::vector<stable_element<T>>, stable_compare<T, Compare>>
    {
        using stableT = stable_element<T>;
        using std::priority_queue<stableT, std::vector<stable_element<T>>, stable_compare<T, Compare>>::priority_queue;

    public:
        const T &top() const { return this->c.front().first; }
        T &top() { return this->c.front().first; }

        void push(const T& value) {
            this->c.push_back(stableT{value, m_counter++});
            std::push_heap(this->c.begin(), this->c.end(), this->comp);
        }

        void push(T&& value) {
            this->c.push_back(stableT{std::move(value), m_counter++});
            std::push_heap(this->c.begin(), this->c.end(), this->comp);
        }
        
        template<class ... Args>
        void emplace(Args&&... args) {
            this->c.emplace_back(T{std::forward<Args>(args)...}, m_counter++);
            std::push_heap(this->c.begin(), this->c.end(), this->comp);
        }

        void pop() {
            std::pop_heap(this->c.begin(), this->c.end(), this->comp);
            this->c.pop_back();
            if (this->empty()) m_counter = 0;
        }

    protected:
        std::size_t m_counter = 0;
    };
    
}

#endif