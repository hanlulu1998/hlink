//
// Created by KINGE on 2025/12/16.
//

#include "buffer.hpp"

#include <algorithm>
#include <vector>
#include <string>
#include <type_traits>
#include <endian.h>
#include <cassert>
#include <cstring>
#include <cerrno>
#include <sys/uio.h>
/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      read_pos   <=       write_pos    <=      buffer.size
/// @endcode


class hlink::Buffer::Impl {
public:
    explicit Impl(size_t initial_size = INITIAL_SIZE);

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

    ssize_t append_from_fd(int fd, int *error);


    template<Integral T>
    T peek_int() {
        T host_value = 0;
        memcpy(&host_value, read_ptr(), sizeof(T));

        if constexpr (sizeof(T) == sizeof(int64_t)) {
            return ::be64toh(host_value);
        } else if (sizeof(T) == sizeof(int32_t)) {
            return ::be32toh(host_value);
        } else if (sizeof(T) == sizeof(int16_t)) {
            return ::be16toh(host_value);
        } else {
            return host_value;
        }
    }

    template<Integral T>
    void pop_int() {
        pop(sizeof(T));
    }

    template<Integral T>
    T read_int() {
        T value = peek_int<T>();
        pop_int<T>();
        return value;
    }

    template<Integral T>
    void push_int(const T value) {
        if constexpr (sizeof(T) == sizeof(int64_t)) {
            T int_be64 = ::htobe64(value);
            append(&int_be64, sizeof(int_be64));
        } else if (sizeof(T) == sizeof(int32_t)) {
            T int_be32 = ::htobe32(value);
            append(&int_be32, sizeof(int_be32));
        } else if (sizeof(T) == sizeof(int16_t)) {
            T int_be16 = ::htobe16(value);
            append(&int_be16, sizeof(int_be16));
        } else {
            append(&value, sizeof(value));
        }
    }

    void prepend(const void *data, size_t bytes);

    void prepend(const char *data, size_t len);

    void prepend(std::string_view sv);

    template<Integral T>
    void prepend_int(const T value) {
        if constexpr (sizeof(T) == sizeof(int64_t)) {
            T int_be64 = ::htobe64(value);
            prepend(&int_be64, sizeof(int_be64));
        } else if (sizeof(T) == sizeof(int32_t)) {
            T int_be32 = ::htobe32(value);
            prepend(&int_be32, sizeof(int_be32));
        } else if (sizeof(T) == sizeof(int16_t)) {
            T int_be16 = ::htobe16(value);
            prepend(&int_be16, sizeof(int_be16));
        } else {
            prepend(&value, sizeof(value));
        }
    }

    void check_writable_bytes(size_t len);

    void append(const char *data, size_t len);

    void append(const void *data, size_t bytes);

    void append(std::string_view sv);

    template<Integral T>
    void append_int(const T value) {
        if constexpr (sizeof(T) == sizeof(int64_t)) {
            T int_be64 = ::htobe64(value);
            append(&int_be64, sizeof(int_be64));
        } else if (sizeof(T) == sizeof(int32_t)) {
            T int_be32 = ::htobe32(value);
            append(&int_be32, sizeof(int_be32));
        } else if (sizeof(T) == sizeof(int16_t)) {
            T int_be16 = ::htobe16(value);
            append(&int_be16, sizeof(int_be16));
        } else {
            append(&value, sizeof(value));
        }
    }

    [[nodiscard]] size_t buffer_size() const;

    [[nodiscard]] size_t buffer_capacity() const;

    void swap(Impl &other) noexcept;

    Impl clone() const;

    std::string pop_as_string(size_t len);

    std::string pop_all_as_string();

    [[nodiscard]] std::string to_string() const;

    void shrink(size_t reserve);

private:
    std::vector<char> buffer_{std::vector<char>(CHEAP_PREPEND + INITIAL_SIZE)};
    size_t read_pos_{CHEAP_PREPEND};
    size_t write_pos_{CHEAP_PREPEND};
    size_t initial_size_{INITIAL_SIZE};

    void alloc_more_memory(size_t len);

    [[nodiscard]] const char *data_ptr() const;

    [[nodiscard]] char *mutable_data_ptr() const;
};


void hlink::Buffer::Impl::swap(Impl &other) noexcept {
    buffer_.swap(other.buffer_);
    std::swap(read_pos_, other.read_pos_);
    std::swap(write_pos_, other.write_pos_);
}

hlink::Buffer::Impl hlink::Buffer::Impl::clone() const {
    return *this;
}

std::string hlink::Buffer::Impl::pop_as_string(const size_t len) {
    assert(len<= readable_bytes());
    std::string result{read_ptr(), len};
    pop(len);
    return result;
}

std::string hlink::Buffer::Impl::pop_all_as_string() {
    return pop_as_string(readable_bytes());
}

std::string hlink::Buffer::Impl::to_string() const {
    return {read_ptr(), readable_bytes()};
}

void hlink::Buffer::Impl::shrink(const size_t reserve) {
    Impl other;
    other.check_writable_bytes(readable_bytes() + reserve);
    other.append(to_string());
    other.buffer_.shrink_to_fit();
    swap(other);
}

void hlink::Buffer::Impl::prepend(const void *data, const size_t bytes) {
    prepend(static_cast<const char *>(data), bytes);
}

void hlink::Buffer::Impl::prepend(const char *data, const size_t len) {
    // 注意这里是从read_pos的前方拼接
    assert(len <= prependable_bytes());
    read_pos_ -= len;
    std::copy_n(data, len, mutable_read_ptr());
}

void hlink::Buffer::Impl::prepend(const std::string_view sv) {
    prepend(sv.data(), sv.size());
}

char *hlink::Buffer::Impl::mutable_write_ptr() const {
    return mutable_data_ptr() + write_pos_;
}

char *hlink::Buffer::Impl::mutable_read_ptr() const {
    return mutable_data_ptr() + read_pos_;
}

size_t hlink::Buffer::Impl::readable_bytes() const {
    return write_pos_ - read_pos_;
}

size_t hlink::Buffer::Impl::writable_bytes() const {
    return buffer_.size() - write_pos_;
}

size_t hlink::Buffer::Impl::prependable_bytes() const {
    return read_pos_;
}

const char *hlink::Buffer::Impl::write_ptr() const {
    return data_ptr() + write_pos_;
}

const char *hlink::Buffer::Impl::read_ptr() const {
    return data_ptr() + read_pos_;
}

void hlink::Buffer::Impl::pop(const size_t bytes) {
    if (bytes < readable_bytes()) {
        read_pos_ += bytes;
    } else {
        // 如果弹出所有的数据，那么可以直接清空了
        clear();
    }
}

void hlink::Buffer::Impl::pop_util(const char *end) {
    assert(read_ptr()<=end);
    assert(write_ptr()>=end);
    pop(end - read_ptr());
}

void hlink::Buffer::Impl::clear() {
    write_pos_ = CHEAP_PREPEND;
    read_pos_ = CHEAP_PREPEND;

    if (buffer_.size() > MAX_BUFFER_THRESHOLD) {
        buffer_.resize(initial_size_ + CHEAP_PREPEND);
        buffer_.shrink_to_fit();
    }
}

void hlink::Buffer::Impl::write_seek(const ssize_t len) {
    if (len > 0) {
        assert(len<= writable_bytes());
    } else {
        assert(-len<= readable_bytes());
    }
    write_pos_ += len;
}

ssize_t hlink::Buffer::Impl::append_from_fd(const int fd, int *error) {
    char extra_buffer[EXTRA_BUFFER_SIZE]{};
    iovec vec[2];
    const size_t writable = writable_bytes();
    vec[0].iov_base = mutable_write_ptr();
    vec[0].iov_len = writable;

    vec[1].iov_base = extra_buffer;
    vec[1].iov_len = sizeof (extra_buffer);

    // 如果buffer中可写的部分小于EXTRA_BUFFER_SIZE（65535）
    // 超出的部分我们放入额外数组，一般都是够用的
    const int iovcnt = writable < sizeof(extra_buffer) ? 2 : 1;

    const ssize_t n = readv(fd, vec, iovcnt);
    if (n < 0) {
        *error = errno;
    } else if (n <= writable) {
        write_seek(n);
    } else {
        // 剩余的不够写，说明启用了额外数组，我们需要把额外的给拷贝到buffer中
        write_pos_ = buffer_size();
        append(extra_buffer, n - writable);
    }
    return n;
}

void hlink::Buffer::Impl::check_writable_bytes(const size_t len) {
    if (len > writable_bytes()) {
        alloc_more_memory(len);
    }
    assert(writable_bytes()>=len);
}

void hlink::Buffer::Impl::append(const char *data, const size_t len) {
    check_writable_bytes(len);
    std::copy_n(data, len, mutable_write_ptr());
    write_seek(len);
}

void hlink::Buffer::Impl::append(const void *data, const size_t bytes) {
    append(static_cast<const char *>(data), bytes);
}

void hlink::Buffer::Impl::append(const std::string_view sv) {
    append(sv.data(), sv.size());
}

size_t hlink::Buffer::Impl::buffer_size() const {
    return buffer_.size();
}

size_t hlink::Buffer::Impl::buffer_capacity() const {
    return buffer_.capacity();
}

void hlink::Buffer::Impl::alloc_more_memory(const size_t len) {
    // buffer的空闲大小不够重新整理
    if (writable_bytes() + prependable_bytes() < len + CHEAP_PREPEND) {
        buffer_.resize(write_pos_ + len);
    } else {
        assert(CHEAP_PREPEND<read_pos_);
        const size_t readable = readable_bytes();
        std::copy(read_ptr(), write_ptr(), mutable_data_ptr() + CHEAP_PREPEND);
        read_pos_ = CHEAP_PREPEND;
        write_pos_ = read_pos_ + readable;
        assert(readable == readable_bytes());
    }
}

const char *hlink::Buffer::Impl::data_ptr() const {
    return buffer_.data();
}

char *hlink::Buffer::Impl::mutable_data_ptr() const {
    return const_cast<char *>(data_ptr());
}

hlink::Buffer::Buffer(const size_t initial_size) : impl_(std::make_unique<Impl>(initial_size)) {
}

hlink::Buffer::Buffer(Buffer &&other) noexcept : impl_(std::move(other.impl_)) {
}

hlink::Buffer &hlink::Buffer::operator=(Buffer &&other) noexcept {
    impl_ = std::move(other.impl_);
    return *this;
}

hlink::Buffer::Impl::Impl(const size_t initial_size) : buffer_(CHEAP_PREPEND + initial_size),
                                                       initial_size_(initial_size) {
}

hlink::Buffer::~Buffer() = default;

size_t hlink::Buffer::readable_bytes() const {
    return impl_->readable_bytes();
}

size_t hlink::Buffer::writable_bytes() const {
    return impl_->writable_bytes();
}

size_t hlink::Buffer::prependable_bytes() const {
    return impl_->prependable_bytes();
}

const char *hlink::Buffer::write_ptr() const {
    return impl_->write_ptr();
}

const char *hlink::Buffer::read_ptr() const {
    return impl_->read_ptr();
}

char *hlink::Buffer::mutable_write_ptr() const {
    return impl_->mutable_write_ptr();
}

char *hlink::Buffer::mutable_read_ptr() const {
    return impl_->mutable_read_ptr();
}

void hlink::Buffer::pop(const size_t bytes) {
    impl_->pop(bytes);
}

void hlink::Buffer::pop_util(const char *end) {
    impl_->pop_util(end);
}

void hlink::Buffer::clear() {
    impl_->clear();
}

void hlink::Buffer::write_seek(const ssize_t len) {
    impl_->write_seek(len);
}

ssize_t hlink::Buffer::append_from_fd(const int fd, int *error) {
    return impl_->append_from_fd(fd, error);
}

int64_t hlink::Buffer::peek_in64t() {
    return impl_->peek_int<int64_t>();
}

int32_t hlink::Buffer::peek_int32t() {
    return impl_->peek_int<int32_t>();
}

int16_t hlink::Buffer::peek_int16t() {
    return impl_->peek_int<int16_t>();
}

int8_t hlink::Buffer::peek_int8t() {
    return impl_->peek_int<int8_t>();
}

void hlink::Buffer::pop_int64t() {
    impl_->pop_int<int64_t>();
}

void hlink::Buffer::pop_int32t() {
    impl_->pop_int<int32_t>();
}

void hlink::Buffer::pop_int16t() {
    impl_->pop_int<int16_t>();
}

void hlink::Buffer::pop_int8t() {
    impl_->pop_int<int8_t>();
}

int64_t hlink::Buffer::read_int64t() {
    return impl_->read_int<int64_t>();
}

int32_t hlink::Buffer::read_int32t() {
    return impl_->read_int<int32_t>();
}

int16_t hlink::Buffer::read_int16t() {
    return impl_->read_int<int16_t>();
}

int8_t hlink::Buffer::read_int8t() {
    return impl_->read_int<int8_t>();
}

void hlink::Buffer::push_int64t(const int64_t value) {
    impl_->push_int(value);
}

void hlink::Buffer::push_int32t(const int32_t value) {
    impl_->push_int(value);
}

void hlink::Buffer::push_int16t(const int16_t value) {
    impl_->push_int(value);
}

void hlink::Buffer::push_int8t(const int8_t value) {
    impl_->push_int(value);
}

void hlink::Buffer::prepend_int64t(const int64_t value) {
    impl_->prepend_int(value);
}

void hlink::Buffer::prepend_int32t(const int32_t value) {
    impl_->prepend_int(value);
}

void hlink::Buffer::prepend_int16t(const int16_t value) {
    impl_->prepend_int(value);
}

void hlink::Buffer::prepend_int8t(const int8_t value) {
    impl_->prepend_int(value);
}

void hlink::Buffer::append_int64t(const int64_t value) {
    impl_->append_int(value);
}

void hlink::Buffer::append_int32t(const int32_t value) {
    impl_->append_int(value);
}

void hlink::Buffer::append_int16t(const int16_t value) {
    impl_->append_int(value);
}

void hlink::Buffer::append_int8t(const int8_t value) {
    impl_->append_int(value);
}

void hlink::Buffer::prepend(const void *data, const size_t bytes) {
    impl_->prepend(data, bytes);
}

void hlink::Buffer::prepend(const std::span<std::byte> data) {
    prepend(data.data(), data.size());
}

void hlink::Buffer::prepend(const char *data, const size_t len) {
    impl_->prepend(data, len);
}

void hlink::Buffer::prepend(const std::string_view sv) {
    impl_->prepend(sv.data(), sv.size());
}

void hlink::Buffer::check_writable_bytes(const size_t len) {
    impl_->check_writable_bytes(len);
}

void hlink::Buffer::append(const char *data, const size_t len) {
    impl_->append(data, len);
}

void hlink::Buffer::append(const void *data, const size_t bytes) {
    impl_->append(data, bytes);
}

void hlink::Buffer::append(const std::span<std::byte> data) {
    append(data.data(), data.size());
}

void hlink::Buffer::append(const std::string_view sv) {
    impl_->append(sv);
}

size_t hlink::Buffer::buffer_size() const {
    return impl_->buffer_size();
}

size_t hlink::Buffer::buffer_capacity() const {
    return impl_->buffer_capacity();
}

void hlink::Buffer::swap(Buffer &other) noexcept {
    impl_.swap(other.impl_);
}

std::string hlink::Buffer::pop_as_string(const size_t len) {
    return impl_->pop_as_string(len);
}

std::string hlink::Buffer::pop_all_as_string() {
    return impl_->pop_all_as_string();
}

std::string hlink::Buffer::to_string() const {
    return impl_->to_string();
}

void hlink::Buffer::shrink(const size_t reserve) {
    impl_->shrink(reserve);
}

hlink::Buffer hlink::Buffer::clone() const {
    Buffer buffer;
    buffer.impl_ = std::make_unique<Impl>(impl_->clone());
    return buffer;
}
