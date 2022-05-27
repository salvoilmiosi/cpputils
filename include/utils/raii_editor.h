#ifndef __RAII_EDITOR__
#define __RAII_EDITOR__

#include <vector>

template<typename T>
class raii_editor {
private:
    T *m_ptr = nullptr;
    T m_prev_value;

public:
    raii_editor() = default;
    
    raii_editor(T &value, const T &new_value)
        : m_ptr(&value)
        , m_prev_value(value)
    {
        value = new_value;
    }
    
    ~raii_editor() {
        if (m_ptr) {
            *m_ptr = std::move(m_prev_value);
            m_ptr = nullptr;
        }
    }

    raii_editor(raii_editor &&other) noexcept
        : m_ptr(std::exchange(other.m_ptr, nullptr))
        , m_prev_value(std::move(other.m_prev_value)) {}

    raii_editor(const raii_editor &other) = delete;

    raii_editor &operator = (raii_editor &&other) noexcept {
        std::swap(m_ptr, other.m_ptr);
        std::swap(m_prev_value, other.m_prev_value);
        return *this;
    }

    raii_editor &operator = (const raii_editor &other) = delete;
};

template<typename T>
class raii_editor_stack {
private:
    std::vector<raii_editor<T>> m_data;

public:
    raii_editor_stack() = default;

    ~raii_editor_stack() {
        clear();
    }

    raii_editor_stack(const raii_editor_stack &) = delete;
    raii_editor_stack &operator = (const raii_editor_stack &) = delete;

    raii_editor_stack(raii_editor_stack &&) = delete;
    raii_editor_stack &operator = (raii_editor_stack &&other) noexcept {
        std::swap(m_data, other.m_data);
        return *this;
    }

    void add(T &value, const T &new_value) {
        m_data.emplace_back(value, new_value);
    }

    void remove() {
        m_data.pop_back();
    }

    bool empty() const {
        return m_data.empty();
    }

    void clear() {
        while (!empty()) {
            remove();
        }
    }
};

#endif