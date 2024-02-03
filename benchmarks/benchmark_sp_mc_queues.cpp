#include <iostream>
#include <cassert>
#include <type_traits>

#include "bounded_multicast_queue.h"

namespace {

    struct Message {
        constexpr Message() = default;
        constexpr explicit Message(int x) : x_(x) {}
        constexpr ~Message() = default;

        int x_{0};
    };

    static_assert(std::is_trivially_destructible_v<Message>);
    static_assert(std::is_trivially_copyable_v<Message>);
    static_assert(concurrent::utils::IsTriviallyCopyableAndDestructible<Message>);

}

int main() {
    const size_t capacity = 1024;

    using Queue = concurrent::queue::BoundedMulticastQueue<capacity, sizeof(Message), alignof(Message)>;
    using Reader = Queue::Reader;
    using Writer = Queue::Writer;

    concurrent::queue::BoundedMulticastQueue<capacity, sizeof(Message), alignof(Message)> q{};
    auto writer_thread = std::thread([&q]() {
        Writer writer{&q};
        for (int i = 0; i < 100; i++) {
            Message message{i};
            writer.Write(i);
        }
    });
    auto reader_thread = std::thread([&q]() {
        Reader reader{&q};
        Message result{};
        for (int i = 0; i < 100; i++) {
            assert(reader.Read(result));
            assert(result.x_ == i);
            reader.UpdateIndexes();
        }
    });

    writer_thread.join();
    reader_thread.join();
    return 0;
}