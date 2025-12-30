//
// Created by KINGE on 2025/12/16.
//
#pragma once
#include <memory>
#include <span>

namespace hlink {
    template<typename T>
    concept Integral = std::is_integral_v<T>;

    class Buffer {
    public:
        static constexpr size_t CHEAP_PREPEND = 8;
        static constexpr size_t INITIAL_SIZE = 1024;
        static constexpr size_t EXTRA_BUFFER_SIZE = 65536;
        static constexpr size_t MAX_BUFFER_THRESHOLD = 65536;

        explicit Buffer(size_t initial_size = INITIAL_SIZE);

        Buffer(const Buffer &) = delete;

        Buffer(Buffer &&other) noexcept;

        Buffer &operator=(const Buffer &other) = delete;


        Buffer &operator=(Buffer &&other) noexcept;


        ~Buffer();

        [[nodiscard]] size_t readable_bytes() const;

        [[nodiscard]] size_t writable_bytes() const;

        [[nodiscard]] size_t prependable_bytes() const;

        [[nodiscard]] const char *write_ptr() const;

        [[nodiscard]] const char *read_ptr() const;

        [[nodiscard]] char *mutable_write_ptr() const;

        [[nodiscard]] char *mutable_read_ptr() const;

        void pop(size_t bytes);

        void pop_util(const char *end);

        void clear();

        void write_seek(ssize_t len);

        [[nodiscard]] ssize_t append_from_fd(int fd, int *error);

        [[nodiscard]] int64_t peek_in64t();

        [[nodiscard]] int32_t peek_int32t();

        [[nodiscard]] int16_t peek_int16t();

        [[nodiscard]] int8_t peek_int8t();


        void pop_int64t();

        void pop_int32t();

        void pop_int16t();

        void pop_int8t();

        [[nodiscard]] int64_t read_int64t();

        [[nodiscard]] int32_t read_int32t();

        [[nodiscard]] int16_t read_int16t();

        [[nodiscard]] int8_t read_int8t();


        void push_int64t(int64_t value);

        void push_int32t(int32_t value);

        void push_int16t(int16_t value);

        void push_int8t(int8_t value);


        void prepend(const void *data, size_t bytes);

        void prepend(std::span<std::byte> data);

        void prepend(const char *data, size_t len);

        void prepend(std::string_view sv);

        void prepend_int64t(int64_t value);

        void prepend_int32t(int32_t value);

        void prepend_int16t(int16_t value);

        void prepend_int8t(int8_t value);

        void check_writable_bytes(size_t len);

        void append(const char *data, size_t len);

        void append(const void *data, size_t bytes);

        void append(std::span<std::byte> data);

        void append(std::string_view sv);

        void append_int64t(int64_t value);

        void append_int32t(int32_t value);

        void append_int16t(int16_t value);

        void append_int8t(int8_t value);

        [[nodiscard]] size_t buffer_size() const;

        [[nodiscard]] size_t buffer_capacity() const;

        // 这里的交换单纯是指针交换
        void swap(Buffer &other) noexcept;

        [[nodiscard]] std::string pop_as_string(size_t len);

        [[nodiscard]] std::string pop_all_as_string();

        [[nodiscard]] std::string to_string() const;

        void shrink(size_t reserve);

        [[nodiscard]] Buffer clone() const;

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };
}
