#ifndef __NULLABLE_H__
#define __NULLABLE_H__

template<typename T>
struct nullable {
    T *value = nullptr;

    nullable() = default;

    explicit nullable(T *value) : value(value) {}

    operator bool() const {
        return value != nullptr;
    }

    T &operator *() const {
        return *value;
    }

    T *operator ->() const {
        return value;
    }

    operator T *() const {
        return value;
    }

    bool operator == (const nullable &other) const = default;
};

template<typename T> nullable(T *) -> nullable<T>;

#endif