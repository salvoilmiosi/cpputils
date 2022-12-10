#ifndef __CONCAT_VIEW_H__
#define __CONCAT_VIEW_H__

#include <ranges>
#include <variant>
#include <tuple>

namespace util {
    
    template <size_t I, typename T> struct indexed {
        T value;

        bool operator == (const indexed &other) const = default;
    };

    template<typename ISeq, typename ... Ts> struct make_index_variant;

    template<typename ... Ts, size_t ... Is>
    struct make_index_variant<std::index_sequence<Is...>, Ts ...> {
        using type = std::variant<indexed<Is, Ts> ...>;
    };

    template<typename ... Ts>
    struct index_variant : make_index_variant<std::index_sequence_for<Ts...>, Ts...> {};

    template<typename ... Ts> using index_variant_t = typename index_variant<Ts...>::type;

    template<typename ... Ts> class concat_view;

    template<typename ... Ts>
    class concat_view_iterator {
    public:
        using iterator_variant = index_variant_t<std::ranges::iterator_t<Ts> ...>;
        using iterator_category = std::input_iterator_tag;
        using value_type = std::common_type_t<std::ranges::range_value_t<Ts>...>;
        using difference_type = std::ptrdiff_t;

        explicit concat_view_iterator() = default;

        template<typename ... Us>
        concat_view_iterator(concat_view<Ts ...> *view, Us && ... arg)
            : m_value(std::forward<Us>(arg) ...)
            , m_view(view) {}
        
        bool operator==(const concat_view_iterator &other) const noexcept = default;

        inline concat_view_iterator &operator++();
        void operator++(int) {
            auto copy = *this;
            ++(*this);
            return copy;
        }

        const value_type &operator*() const {
            return std::visit<const value_type &>([](const auto &it) -> decltype(auto) {
                return *it.value;
            }, m_value);
        }
        inline const value_type *operator->() const {
            return &(*this);
        }
    
    private:
        iterator_variant m_value;
        concat_view<Ts ...> *m_view = nullptr;
    };

    template<typename ... Ts>
    class concat_view {
    public:
        using iterator = concat_view_iterator<Ts...>;

        template<typename ... Us>
        concat_view(Us && ... ranges)
            : m_value{std::forward<Us>(ranges) ... } {}
        
        auto begin() {
            if (!m_begin) m_begin = find_begin<0>();
            return *m_begin;
        }

        auto end() {
            static constexpr auto last = sizeof...(Ts) - 1;
            return iterator(this, std::in_place_index<last>, std::ranges::end(std::get<last>(m_value)));
        }

    private:
        std::tuple<Ts ...> m_value;

        std::optional<iterator> m_begin;

        template<size_t I>
        iterator find_begin() {
            auto &range = std::get<I>(m_value);
            auto it = std::ranges::begin(range);
            if constexpr (I == sizeof...(Ts) - 1) {
                return iterator(this, std::in_place_index<I>, std::move(it));
            } else if (it != std::ranges::end(range)) {
                return iterator(this, std::in_place_index<I>, std::move(it));
            } else {
                return find_begin<I + 1>();
            }
        }

        friend iterator;
    };

    template<typename ... Ts>
    inline concat_view_iterator<Ts...> &concat_view_iterator<Ts...>::operator++() {
        std::visit([&]<size_t I, typename T>(indexed<I, T> &it){
            ++it.value;
            if constexpr (I != sizeof...(Ts) - 1) {
                if (it.value == std::ranges::end(std::get<I>(m_view->m_value))) {
                    *this = m_view->template find_begin<I + 1>();
                }
            }
        }, m_value);
        return *this;
    }

    template<typename ... Ts> concat_view(Ts && ...) -> concat_view<Ts ...>;

}

#endif