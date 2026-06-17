#pragma once

#include <cstddef>
#include <new>
#include <utility>

namespace minisql {

template <typename T>
class Array {
public:
    Array() = default;

    Array(const Array& other) {
        assign_from(other);
    }

    Array(Array&& other) noexcept
        : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    ~Array() {
        destroy();
    }

    Array& operator=(const Array& other) {
        if (this != &other) {
            destroy();
            assign_from(other);
        }
        return *this;
    }

    Array& operator=(Array&& other) noexcept {
        if (this != &other) {
            destroy();
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }

    void push_back(const T& value) {
        ensure_capacity(size_ + 1);
        new (data_ + size_) T(value);
        ++size_;
    }

    void push_back(T&& value) {
        ensure_capacity(size_ + 1);
        new (data_ + size_) T(std::move(value));
        ++size_;
    }

    void erase(std::size_t index) {
        if (index >= size_) {
            return;
        }
        data_[index].~T();
        for (std::size_t i = index; i + 1 < size_; ++i) {
            new (data_ + i) T(std::move(data_[i + 1]));
            data_[i + 1].~T();
        }
        --size_;
    }

    void clear() {
        for (std::size_t i = 0; i < size_; ++i) {
            data_[i].~T();
        }
        size_ = 0;
    }

    [[nodiscard]] std::size_t size() const {
        return size_;
    }

    [[nodiscard]] bool empty() const {
        return size_ == 0;
    }

    T& operator[](std::size_t index) {
        return data_[index];
    }

    const T& operator[](std::size_t index) const {
        return data_[index];
    }

    T* begin() {
        return data_;
    }

    const T* begin() const {
        return data_;
    }

    T* end() {
        return data_ + size_;
    }

    const T* end() const {
        return data_ + size_;
    }

private:
    void assign_from(const Array& other) {
        ensure_capacity(other.size_);
        for (std::size_t i = 0; i < other.size_; ++i) {
            new (data_ + i) T(other.data_[i]);
        }
        size_ = other.size_;
    }

    void ensure_capacity(std::size_t required) {
        if (required <= capacity_) {
            return;
        }
        std::size_t next_capacity = capacity_ == 0 ? 4 : capacity_ * 2;
        while (next_capacity < required) {
            next_capacity *= 2;
        }
        T* next = static_cast<T*>(::operator new(sizeof(T) * next_capacity));
        for (std::size_t i = 0; i < size_; ++i) {
            new (next + i) T(std::move(data_[i]));
            data_[i].~T();
        }
        ::operator delete(data_);
        data_ = next;
        capacity_ = next_capacity;
    }

    void destroy() {
        clear();
        ::operator delete(data_);
        data_ = nullptr;
        capacity_ = 0;
    }

    T* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};

}  // namespace minisql
