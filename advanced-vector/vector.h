#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <type_traits>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;

    RawMemory& operator= (const RawMemory&) = delete;

    RawMemory(RawMemory&& other) noexcept {
        std::swap(capacity_, other.capacity_);
        std::swap(buffer_, other.buffer_);
    }

    RawMemory& operator= (RawMemory&& rhs) noexcept {
        std::swap(capacity_, rhs.capacity_);
        std::swap(buffer_, rhs.buffer_);
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;
    
    iterator begin() noexcept {
        return data_ + 0;
    }
    iterator end() noexcept {
        return data_ + size_;
    }
    const_iterator begin() const noexcept {
        return data_ + 0;
    }
    const_iterator end() const noexcept { 
        return data_ + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_ + 0;
    }
    const_iterator cend() const noexcept {
        return data_ + size_;
    }

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)  //
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }
    
    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)  //
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }
    
    Vector(Vector&& other) noexcept {
        data_ = std::move(other.data_);
        std::swap(size_, other.size_);
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector copy_rhs{rhs};
                Swap(copy_rhs);
            } else {
                if (rhs.size_ < size_) {
                    for (size_t i = 0; i < rhs.size_; ++i) {
                        data_[i] = rhs[i];
                    }
                    std::destroy_n(data_ + rhs.size_, size_ - rhs.size_);
                } else {
                    for (size_t i = 0; i < size_; ++i) {
                        data_[i] = rhs[i];
                    }
                    std::uninitialized_copy_n(rhs.data_ + size_, rhs.size_ - size_, data_ + size_);
                }
                size_ = rhs.size_;
            }    
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        data_ = std::move(rhs.data_);
        std::swap(size_, rhs.size_);
        return *this;
    }

    ~Vector() {
        DestroyN(data_.GetAddress(), size_);
    }
    
    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }
    
    void Resize(size_t new_size) {
        if (new_size > size_) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
        } else if (new_size < size_) {
            std::destroy_n(data_ + new_size, size_ - new_size);
        }
        size_ = new_size;
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::forward<T>(value));
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + size_) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        } else {
            new (data_ + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return data_[size_ - 1];
    }

    iterator Erase(const_iterator pos) {
        size_t dist = std::distance(cbegin(), pos);
        auto cur = begin() + dist;
        Destroy(std::move(cur + 1, end(), cur));
        --size_;
        return cur; 
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        size_t dist = std::distance(cbegin(), pos);
        auto it = begin() + dist;
        if (size_ == data_.Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data.GetAddress() + dist) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), dist , new_data.GetAddress());
                std::uninitialized_move_n(it, size_ - dist, new_data.GetAddress() + dist + 1);
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), dist, new_data.GetAddress());
                std::uninitialized_copy_n(it, size_ - dist, new_data.GetAddress() + dist + 1);
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        } else {
            if (size_ != 0) {
                T temp{std::forward<Args>(args)...};
                if (size_ != 0) {
                    new (end()) T(std::forward<T>(data_[size_ - 1]));
                }
                std::move_backward(it, end() - 1, end());
                *it = std::forward<T>(temp);
            } else {
                new (it) T(std::forward<Args>(args)...);
            }
        }
        ++size_;
        return data_ + dist;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::forward<T>(value));
    }

    void PopBack() {
        --size_;
        (data_ + size_)->~T();
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

private:
    RawMemory<T> data_{0};
    size_t size_ = 0;
    
    // Вызывает деструкторы n объектов массива по адресу buf
    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    // Создаёт копию объекта elem в сырой памяти по адресу buf
    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    // Вызывает деструктор объекта по адресу buf
    static void Destroy(T* buf) noexcept {
        buf->~T();
    }
};