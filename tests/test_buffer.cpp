//
// Created by KINGE on 2025/12/16.
//
#include "buffer.hpp"
#include <gtest/gtest.h>


using namespace hlink;
TEST(BufferCase, TestAppendAndPop) {
    Buffer buffer;

    ASSERT_EQ(buffer.readable_bytes(), 0);
    ASSERT_EQ(buffer.writable_bytes(), Buffer::INITIAL_SIZE);
    ASSERT_EQ(buffer.prependable_bytes(), Buffer::CHEAP_PREPEND);


    const std::string str(200, 'x');

    buffer.append(str);

    ASSERT_EQ(buffer.readable_bytes(), str.size());
    ASSERT_EQ(buffer.writable_bytes(), Buffer::INITIAL_SIZE - str.size());
    ASSERT_EQ(buffer.prependable_bytes(), Buffer::CHEAP_PREPEND);

    const std::string str2 = buffer.pop_as_string(50);

    ASSERT_EQ(str2.size(), 50);
    ASSERT_EQ(buffer.readable_bytes(), str.size() - str2.size());
    ASSERT_EQ(buffer.writable_bytes(), Buffer::INITIAL_SIZE - str.size());
    ASSERT_EQ(buffer.prependable_bytes(), Buffer::CHEAP_PREPEND + str2.size());
    ASSERT_EQ(str2, std::string(50, 'x'));


    buffer.append(str);

    ASSERT_EQ(buffer.readable_bytes(), str.size() * 2 - str2.size());
    ASSERT_EQ(buffer.writable_bytes(), Buffer::INITIAL_SIZE - str.size() * 2);
    ASSERT_EQ(buffer.prependable_bytes(), Buffer::CHEAP_PREPEND + str2.size());

    const std::string str3 = buffer.pop_all_as_string();
    ASSERT_EQ(str3.size(), 350);
    ASSERT_EQ(buffer.readable_bytes(), 0);
    ASSERT_EQ(buffer.writable_bytes(), Buffer::INITIAL_SIZE);
    ASSERT_EQ(buffer.prependable_bytes(), Buffer::CHEAP_PREPEND);
}

TEST(BufferCase, TestBufferGrow) {
    Buffer buffer;

    buffer.append(std::string(400, 'y'));

    ASSERT_EQ(buffer.readable_bytes(), 400);
    ASSERT_EQ(buffer.writable_bytes(), Buffer::INITIAL_SIZE - 400);

    buffer.pop(50);
    ASSERT_EQ(buffer.readable_bytes(), 350);
    ASSERT_EQ(buffer.writable_bytes(), Buffer::INITIAL_SIZE - 400);
    ASSERT_EQ(buffer.prependable_bytes(), Buffer::CHEAP_PREPEND + 50);

    buffer.append(std::string(1000, 'z'));

    ASSERT_EQ(buffer.readable_bytes(), 1350);
    ASSERT_EQ(buffer.writable_bytes(), 0);
    ASSERT_EQ(buffer.prependable_bytes(), Buffer::CHEAP_PREPEND + 50);

    buffer.pop(1350);
    ASSERT_EQ(buffer.readable_bytes(), 0);

    GTEST_LOG_(INFO) << "buff.writeable_bytes() = " << buffer.writable_bytes();
    ASSERT_EQ(buffer.writable_bytes(), 1400);
    ASSERT_EQ(buffer.prependable_bytes(), Buffer::CHEAP_PREPEND);
}

TEST(BufferCase, TestBufferInsideGrow) {
    Buffer buffer;

    buffer.append(std::string(800, 'y'));

    ASSERT_EQ(buffer.readable_bytes(), 800);
    ASSERT_EQ(buffer.writable_bytes(), Buffer::INITIAL_SIZE - 800);


    buffer.pop(500);
    ASSERT_EQ(buffer.readable_bytes(), 300);
    ASSERT_EQ(buffer.writable_bytes(), Buffer::INITIAL_SIZE - 800);
    ASSERT_EQ(buffer.prependable_bytes(), Buffer::CHEAP_PREPEND + 500);


    buffer.append(std::string(300, 'z'));
    ASSERT_EQ(buffer.readable_bytes(), 600);
    ASSERT_EQ(buffer.writable_bytes(), Buffer::INITIAL_SIZE - 600);
    ASSERT_EQ(buffer.prependable_bytes(), Buffer::CHEAP_PREPEND);
}

TEST(BufferCase, TestBufferShrink) {
    Buffer buffer;
    buffer.append(std::string(2000, 'z'));
    ASSERT_EQ(buffer.readable_bytes(), 2000);
    ASSERT_EQ(buffer.writable_bytes(), 0);
    ASSERT_EQ(buffer.prependable_bytes(), Buffer::CHEAP_PREPEND);

    buffer.pop(1500);
    ASSERT_EQ(buffer.readable_bytes(), 500);
    ASSERT_EQ(buffer.writable_bytes(), 0);
    ASSERT_EQ(buffer.prependable_bytes(), Buffer::CHEAP_PREPEND + 1500);


    buffer.shrink(0);
    ASSERT_EQ(buffer.readable_bytes(), 500);
    ASSERT_EQ(buffer.writable_bytes(), Buffer::INITIAL_SIZE - 500);
    ASSERT_EQ(buffer.pop_all_as_string(), std::string(500, 'z'));
    ASSERT_EQ(buffer.prependable_bytes(), Buffer::CHEAP_PREPEND);
}


TEST(BufferCase, TestBufferPrepend) {
    Buffer buffer;
    buffer.append(std::string(200, 'y'));

    ASSERT_EQ(buffer.readable_bytes(), 200);
    ASSERT_EQ(buffer.writable_bytes(), Buffer::INITIAL_SIZE - 200);
    ASSERT_EQ(buffer.prependable_bytes(), Buffer::CHEAP_PREPEND);

    constexpr int x = 0;

    buffer.prepend(&x, sizeof(x));

    ASSERT_EQ(buffer.readable_bytes(), 204);
    ASSERT_EQ(buffer.writable_bytes(), Buffer::INITIAL_SIZE - 200);
    ASSERT_EQ(buffer.prependable_bytes(), Buffer::CHEAP_PREPEND - 4);
}

TEST(BufferCase, TestBufferReadInt) {
    Buffer buffer;
    buffer.append("HTTP");

    ASSERT_EQ(buffer.readable_bytes(), 4);
    ASSERT_EQ(buffer.peek_int8t(), 'H');
    int top16 = buffer.peek_int16t();
    ASSERT_EQ(top16, 'H' * 256 + 'T');
    ASSERT_EQ(buffer.peek_int32t(), top16 * 65536 + 'T' * 256 + 'P');

    ASSERT_EQ(buffer.read_int8t(), 'H');
    ASSERT_EQ(buffer.read_int16t(), 'T' * 256 + 'T');
    ASSERT_EQ(buffer.read_int8t(), 'P');
    ASSERT_EQ(buffer.readable_bytes(), 0);
    ASSERT_EQ(buffer.writable_bytes(), Buffer::INITIAL_SIZE);

    buffer.append_int8t(-1);
    buffer.append_int16t(-2);
    buffer.append_int32t(-3);

    ASSERT_EQ(buffer.readable_bytes(), 7);
    ASSERT_EQ(buffer.read_int8t(), -1);
    ASSERT_EQ(buffer.read_int16t(), -2);
    ASSERT_EQ(buffer.read_int32t(), -3);
    ASSERT_EQ(buffer.readable_bytes(), 0);
    ASSERT_EQ(buffer.writable_bytes(), Buffer::INITIAL_SIZE);
}

TEST(BufferCase, TestBufferMove) {
    auto test_move = [](Buffer &&buffer, const void *inner) {
        const Buffer new_buffer(std::move(buffer));
        ASSERT_EQ(new_buffer.read_ptr(), inner);
    };
    Buffer buffer;
    buffer.append("hlink", 5);
    const void *inner = buffer.read_ptr();
    test_move(std::move(buffer), inner);
}

TEST(BufferCase, TestBufferClone) {
    Buffer buffer;
    buffer.append("HTTP");

    ASSERT_EQ(buffer.readable_bytes(), 4);
    ASSERT_EQ(buffer.peek_int8t(), 'H');
    int top16 = buffer.peek_int16t();
    ASSERT_EQ(top16, 'H' * 256 + 'T');
    ASSERT_EQ(buffer.peek_int32t(), top16 * 65536 + 'T' * 256 + 'P');

    Buffer new_buffer = buffer.clone();
    new_buffer.append("HTTP");

    ASSERT_EQ(new_buffer.readable_bytes(), 8);
    ASSERT_EQ(new_buffer.peek_int8t(), 'H');
    int new_top16 = new_buffer.peek_int16t();
    ASSERT_EQ(new_top16, 'H' * 256 + 'T');
    ASSERT_EQ(new_buffer.peek_int32t(), new_top16 * 65536 + 'T' * 256 + 'P');

    ASSERT_EQ(buffer.read_int8t(), 'H');
    ASSERT_EQ(buffer.read_int16t(), 'T' * 256 + 'T');
    ASSERT_EQ(buffer.read_int8t(), 'P');
    ASSERT_EQ(buffer.readable_bytes(), 0);
    ASSERT_EQ(buffer.writable_bytes(), Buffer::INITIAL_SIZE);

    buffer.append_int8t(-1);
    buffer.append_int16t(-2);
    buffer.append_int32t(-3);

    ASSERT_EQ(buffer.readable_bytes(), 7);
    ASSERT_EQ(buffer.read_int8t(), -1);
    ASSERT_EQ(buffer.read_int16t(), -2);
    ASSERT_EQ(buffer.read_int32t(), -3);
    ASSERT_EQ(buffer.readable_bytes(), 0);
    ASSERT_EQ(buffer.writable_bytes(), Buffer::INITIAL_SIZE);
}
