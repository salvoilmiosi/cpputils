#ifndef __GENERATOR_H__
#define __GENERATOR_H__

#include <coroutine>
#include <variant>

#include "utils.h"

namespace util {
    struct suspend_maybe { // just a general-purpose helper
        bool ready;
        explicit suspend_maybe(bool ready) : ready(ready) { }
        bool await_ready() const noexcept { return ready; }
        void await_suspend(std::coroutine_handle<>) const noexcept { }
        void await_resume() const noexcept { }
    };

    template<typename T>
    class [[nodiscard]] generator {
    public:
        struct promise_type;
        using handle_type = std::coroutine_handle<promise_type>;

    private:
        handle_type handle;

        explicit generator(handle_type handle) : handle(std::move(handle)) { }
    public:
        class iterator {
        private:
            handle_type handle;
            friend generator;

            explicit iterator(handle_type handle) noexcept : handle(handle) { }

        public:
            // less clutter
            using iterator_category = std::input_iterator_tag;
            using value_type = std::remove_cvref_t<T>;
            using difference_type = std::ptrdiff_t;
            
            explicit iterator() = default;

            // just need the one
            bool operator==(std::default_sentinel_t) const noexcept { return handle.done(); }

            // need to muck around inside promise_type for this, so the definition is pulled out to break the cycle
            inline iterator &operator++();
            void operator++(int) { operator++(); }
            // again, need to see into promise_type
            inline T const *operator->() const noexcept;
            T const &operator*() const noexcept { return *operator->(); }
        };
        iterator begin() noexcept {
            return iterator{handle};
        }
        std::default_sentinel_t end() const noexcept {
            return std::default_sentinel;
        }

        using range_type = std::ranges::subrange<iterator, std::default_sentinel_t>;

        struct promise_type {
            // invariant: whenever the coroutine is non-finally suspended, this is nonempty
            // either the T const* is nonnull or the range_type is nonempty
            // note that neither of these own the data (T object or generator)
            // the coroutine's suspended state is often the actual owner
            std::variant<T const*, range_type> value = nullptr;

            generator get_return_object() {
                return generator(handle_type::from_promise(*this));
            }
            // initially suspending does not play nice with the conventional asymmetry between begin() and end()
            std::suspend_never initial_suspend() { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            void unhandled_exception() { std::terminate(); }
            std::suspend_always yield_value(T const &x) noexcept {
                value = std::addressof(x);
                return {};
            }
            suspend_maybe await_transform(generator &&source) noexcept {
                range_type range(source);
                value = range;
                return suspend_maybe(range.empty());
            }
            void return_void() { }
        };

        generator(generator const&) = delete;
        generator(generator &&other) noexcept : handle(std::move(other.handle)) {
            other.handle = nullptr;
        }
        ~generator() { if(handle) handle.destroy(); }
        generator& operator=(generator const&) = delete;
        generator& operator=(generator &&other) noexcept {
            // idiom: implementing assignment by swapping means the impending destruction/reuse of other implicitly handles cleanup of the resource being thrown away (which originated in *this)
            std::swap(handle, other.handle);
            return *this;
        }
    };

    // these are both recursive because I can't be bothered otherwise
    // feel free to change that if it actually bites
    template<typename T>
    inline auto generator<T>::iterator::operator++() -> iterator& {
        std::visit(overloaded{
            [&](T const *) { handle(); },
            [&](range_type &r) {
                if (r.advance(1).empty()) handle();
            }
        }, handle.promise().value);
        return *this;
    }

    template<typename T>
    inline auto generator<T>::iterator::operator->() const noexcept -> T const* {
        return std::visit(overloaded{
            [](T const *x) { return x; },
            [](range_type &r) {
                return r.begin().operator->();
            }
        }, handle.promise().value);
    }
}

#endif