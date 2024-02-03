#ifndef LOCK_FREE_DATA_STRUCTURES_BOUNDED_SP_MC_QUEUE_H
#define LOCK_FREE_DATA_STRUCTURES_BOUNDED_SP_MC_QUEUE_H

#include <array>
#include <atomic>
#include <type_traits>
#include <cstddef>
#include <cstring>
#include <bit>

#include "cache_line.h"
#include "seq_lock.h"
#include "atomic_memcpy.h"
#include "wait.h"

namespace concurrent::queue {

    // todo prove move && to & and & to &

    template<std::size_t Capacity, std::size_t Alignment = utils::kDefaultAlignment>
    class AtomicMulticastQueueMessage;

    template<std::size_t Capacity, std::size_t Alignment = utils::kDefaultAlignment>
    class MulticastQueueMessage {
    public:
        MulticastQueueMessage() = default;

        template<typename T, typename = std::enable_if_t<utils::IsTriviallyCopyableAndDestructible<T>>>
        explicit MulticastQueueMessage(T message);

        MulticastQueueMessage(const MulticastQueueMessage& other) noexcept;
        MulticastQueueMessage(MulticastQueueMessage&& other) noexcept;
        MulticastQueueMessage& operator=(const MulticastQueueMessage& other) noexcept;
        MulticastQueueMessage& operator=(MulticastQueueMessage&& other) noexcept;

        template<typename T, typename = std::enable_if_t<utils::IsTriviallyCopyableAndDestructible<T>>>
        void Get(T& message);

        ~MulticastQueueMessage() = default;

        friend class concurrent::queue::AtomicMulticastQueueMessage<Capacity, Alignment>;

    private:
        template<typename Message>
        void Copy(Message&& message) noexcept;

    private:
        std::aligned_storage_t<Capacity, Alignment> data_{};
        std::size_t message_size_{0};
    };

    template<std::size_t Capacity, std::size_t Alignment>
    class AtomicMulticastQueueMessage {
    private:
        using SeqLock = concurrent::lock::SeqLock;
        using Counter = concurrent::lock::SeqLock::Counter;

    public:
        AtomicMulticastQueueMessage() = default;

        template<typename T, typename = std::enable_if_t<utils::IsTriviallyCopyableAndDestructible<T>>>
        explicit AtomicMulticastQueueMessage(T message);

        AtomicMulticastQueueMessage(const AtomicMulticastQueueMessage&) = delete;
        AtomicMulticastQueueMessage(AtomicMulticastQueueMessage&&) = delete;
        AtomicMulticastQueueMessage& operator=(const AtomicMulticastQueueMessage&) = delete;
        AtomicMulticastQueueMessage& operator=(AtomicMulticastQueueMessage&&) = delete;

        Counter Load(MulticastQueueMessage<Capacity, Alignment>& loaded_message);

        template<typename T, typename = std::enable_if_t<utils::IsTriviallyCopyableAndDestructible<T>>>
        void Store(T desired_message);

        ~AtomicMulticastQueueMessage() = default;

    private:
        std::aligned_storage_t<Capacity, Alignment> data_{};
        std::atomic<std::size_t> message_size_{0};
        SeqLock seq_lock_{};
    };

    template<std::size_t MessagesCount, std::size_t MaxMessageSize, std::size_t MessageAlignment = utils::kDefaultAlignment>
    class BoundedMulticastQueue {
    private:
        using Message = MulticastQueueMessage<MaxMessageSize, MessageAlignment>;
        using AtomicMessage = AtomicMulticastQueueMessage<MaxMessageSize, MessageAlignment>;

    public:
        BoundedMulticastQueue() = default;

        BoundedMulticastQueue(const BoundedMulticastQueue&) = delete;
        BoundedMulticastQueue(BoundedMulticastQueue&&) = delete;
        BoundedMulticastQueue& operator=(const BoundedMulticastQueue&) = delete;
        BoundedMulticastQueue& operator=(BoundedMulticastQueue&&) = delete;

        ~BoundedMulticastQueue() = default;

        class Writer {
        private:
            using Queue = BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>;

        public:
            explicit Writer(Queue* queue);

            Writer(const Writer&) = delete;
            Writer& operator=(const Writer&) = delete;

            Writer(Writer&& other) noexcept;
            Writer& operator=(Writer&& other) noexcept;

            template<typename T, typename = std::enable_if_t<utils::IsTriviallyCopyableAndDestructible<T>>>
            void Write(T desired_message);

            void Swap(Writer& other) noexcept;

            ~Writer() = default;

        private:
            Queue* queue_{nullptr};
            std::size_t tail_{0};
        };

        class Reader {
        private:
            using Queue = BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>;

        public:
            explicit Reader(Queue* queue);

            Reader(const Reader& other);
            Reader(Reader&& other) noexcept;
            Reader& operator=(const Reader& other);
            Reader& operator=(Reader&& other) noexcept;

            // < 0 - The data was not updated. The reader must wait. (real_seq < expected_seq)
            // == 0 - The expected data version was read. (real_seq == expected_seq)
            // > 0 - The data was overwritten several times. The reader is late. (real_seq > expected_seq)
            int32_t TryRead(Message& message);

            template<typename T, typename = std::enable_if_t<utils::IsTriviallyCopyableAndDestructible<T>>>
            int32_t TryRead(T& message);

            // true - The message was read
            // false - The data was overwritten several times. The reader is late
            bool Read(Message& message);

            template<typename T, typename = std::enable_if_t<utils::IsTriviallyCopyableAndDestructible<T>>>
            bool Read(T& message);

            // The function must be called if the Read() function returned 0
            void UpdateIndexes();

            void Swap(Reader& other) noexcept;

            ~Reader() = default;

        private:
            static constexpr std::size_t GetSeqRightShiftValue();

            Queue* queue_{nullptr};
            std::size_t head_{0};
            std::size_t expected_seq_{2};
        };

        friend class Writer;
        friend class Reader;

    private:
        static constexpr std::size_t GetBufferSize();
        static constexpr std::size_t GetIndexMask();

        std::array<AtomicMessage, GetBufferSize()> buffer_{};
    };


    // Implementation
    template<std::size_t Capacity, std::size_t Alignment>
    template<typename T, typename>
    MulticastQueueMessage<Capacity, Alignment>::MulticastQueueMessage(T message) : message_size_(sizeof(message)) {
        std::memcpy(&data_, reinterpret_cast<char*>(&message), sizeof(message));
    }

    template<std::size_t Capacity, std::size_t Alignment>
    MulticastQueueMessage<Capacity, Alignment>::MulticastQueueMessage(const MulticastQueueMessage& other) noexcept {
        Copy(std::forward<MulticastQueueMessage>(other));
    }

    template<std::size_t Capacity, std::size_t Alignment>
    MulticastQueueMessage<Capacity, Alignment>::MulticastQueueMessage(MulticastQueueMessage&& other) noexcept {
        Copy(std::forward<MulticastQueueMessage>(other));
    }

    template<std::size_t Capacity, std::size_t Alignment>
    MulticastQueueMessage<Capacity, Alignment>& MulticastQueueMessage<Capacity, Alignment>::operator=(const MulticastQueueMessage& other) noexcept {
        if (this != &other) {
            Copy(std::forward<MulticastQueueMessage>(other));
        }
        return *this;
    }

    template<std::size_t Capacity, std::size_t Alignment>
    MulticastQueueMessage<Capacity, Alignment>& MulticastQueueMessage<Capacity, Alignment>::operator=(MulticastQueueMessage&& other) noexcept {
        if (this != &other) {
            Copy(std::forward<MulticastQueueMessage>(other));
        }
        return *this;
    }

    template<std::size_t Capacity, std::size_t Alignment>
    template<typename T, typename>
    void MulticastQueueMessage<Capacity, Alignment>::Get(T& message) {
        std::memcpy(reinterpret_cast<char*>(&message), &data_, sizeof(message));
    }

    template<std::size_t Capacity, std::size_t Alignment>
    template<typename Message>
    void MulticastQueueMessage<Capacity, Alignment>::Copy(Message&& message) noexcept {
        std::memcpy(&data_, &message.data_, message.message_size_);
        message_size_ = message.message_size_;
    }

    // AtomicMulticastQueueMessage
    template<std::size_t Capacity, std::size_t Alignment>
    template<typename T, typename>
    AtomicMulticastQueueMessage<Capacity, Alignment>::AtomicMulticastQueueMessage(T message) : message_size_(sizeof(message)) {
        std::memcpy(&data_, reinterpret_cast<char*>(&message), sizeof(message));
    }

    template<std::size_t Capacity, std::size_t Alignment>
    concurrent::lock::SeqLock::Counter AtomicMulticastQueueMessage<Capacity, Alignment>::Load(MulticastQueueMessage<Capacity, Alignment>& loaded_message) {
        Counter seq0;
        Counter seq1;

        do {
            seq0 = seq_lock_.Load(std::memory_order_acquire);

            loaded_message.message_size_ = message_size_.load(std::memory_order_relaxed);
            memcpy::atomic_memcpy_load(reinterpret_cast<char*>(&loaded_message), &data_, loaded_message.message_size_);
            std::atomic_thread_fence(std::memory_order_acquire);

            seq1 = seq_lock_.Load(std::memory_order_relaxed);
        } while (SeqLock::IsLocked(seq0) || seq0 != seq1);

        return seq0;
    }

    template<std::size_t Capacity, std::size_t Alignment>
    template<typename T, typename>
    void AtomicMulticastQueueMessage<Capacity, Alignment>::Store(T desired_message) {
        Counter seq = seq_lock_.Lock();

        std::atomic_thread_fence(std::memory_order_release);
        memcpy::atomic_memcpy_store(&data_, reinterpret_cast<char*>(&desired_message), sizeof(desired_message));
        message_size_.store(sizeof(desired_message), std::memory_order_relaxed);

        seq_lock_.Unlock(seq);
    }

    // Writer
    template<std::size_t MessagesCount, std::size_t MaxMessageSize, std::size_t MessageAlignment>
    BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>::Writer::Writer(
            BoundedMulticastQueue::Writer::Queue* queue) : queue_(queue)  {}

    template<std::size_t MessagesCount, std::size_t MaxMessageSize, std::size_t MessageAlignment>
    BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>::Writer::Writer(
            BoundedMulticastQueue::Writer&& other) noexcept : queue_(other.queue_), tail_(other.tail_) {
        other.queue_ = nullptr;
        other.tail_ = 0;
    }

    template<std::size_t MessagesCount, std::size_t MaxMessageSize, std::size_t MessageAlignment>
    BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>::Writer& BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>::Writer::operator=(
            BoundedMulticastQueue::Writer&& other) noexcept {
        if (this != &other) {
            Swap(std::move(other));
        }
        return *this;
    }

    template<std::size_t MessagesCount, std::size_t MaxMessageSize, std::size_t MessageAlignment>
    template<typename T, typename>
    void BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>::Writer::Write(T desired_message) {
        queue_->buffer_[tail_].Store(std::forward<T>(desired_message));
        tail_ = (tail_ + 1) & BoundedMulticastQueue::GetIndexMask();
    }

    template<std::size_t MessagesCount, std::size_t MaxMessageSize, std::size_t MessageAlignment>
    void BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>::Writer::Swap(
            BoundedMulticastQueue::Writer& other) noexcept {
        using std::swap;
        swap(queue_, other.queue_);
        swap(tail_, other.tail_);
    }

    // Reader
    template<std::size_t MessagesCount, std::size_t MaxMessageSize, std::size_t MessageAlignment>
    BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>::Reader::Reader(
            BoundedMulticastQueue::Reader::Queue* queue) : queue_(queue) {}

    template<std::size_t MessagesCount, std::size_t MaxMessageSize, std::size_t MessageAlignment>
    BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>::Reader::Reader(
            const BoundedMulticastQueue::Reader& other) : queue_(other.queue_), head_(other.head_) {}

    template<std::size_t MessagesCount, std::size_t MaxMessageSize, std::size_t MessageAlignment>
    BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>::Reader::Reader(
            BoundedMulticastQueue::Reader&& other) noexcept : queue_(other.queue_), head_(other.head_) {
        other.queue_ = nullptr;
        other.head_ = 0;
    }

    template<std::size_t MessagesCount, std::size_t MaxMessageSize, std::size_t MessageAlignment>
    BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>::Reader& BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>::Reader::operator=(
            const BoundedMulticastQueue::Reader& other) {
        if (this != &other) {
            BoundedMulticastQueue::Reader tmp(std::forward<BoundedMulticastQueue::Reader>(other));
            tmp.Swap(*this);
        }
        return *this;
    }

    template<std::size_t MessagesCount, std::size_t MaxMessageSize, std::size_t MessageAlignment>
    BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>::Reader& BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>::Reader::operator=(
            BoundedMulticastQueue::Reader&& other) noexcept {
        if (this != &other) {
            Swap(std::move(other));
        }
        return *this;
    }

    template<std::size_t MessagesCount, std::size_t MaxMessageSize, std::size_t MessageAlignment>
    int32_t BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>::Reader::TryRead(
            BoundedMulticastQueue::Message& message) {
        auto real_seq = queue_->buffer_[head_].Load(message);
        return static_cast<int32_t>(real_seq) - static_cast<int32_t>(expected_seq_);
    }

    template<std::size_t MessagesCount, std::size_t MaxMessageSize, std::size_t MessageAlignment>
    template<typename T, typename>
    int32_t BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>::Reader::TryRead(T& message) {
        Message queue_message{};
        auto result = TryRead(queue_message);
        queue_message.Get(message);
        return result;
    }

    template<std::size_t MessagesCount, std::size_t MaxMessageSize, std::size_t MessageAlignment>
    bool BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>::Reader::Read(
            BoundedMulticastQueue::Message& message) {
        while (true) {
            int32_t result = TryRead(message);
            if (!result) {
                return true;
            } else if (result > 0) {
                return false;
            } else {
                concurrent::wait::Wait();
            }
        }
    }

    template<std::size_t MessagesCount, std::size_t MaxMessageSize, std::size_t MessageAlignment>
    template<typename T, typename>
    bool BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>::Reader::Read(T& message) {
        Message queue_message{};
        auto result = Read(queue_message);
        queue_message.Get(message);
        return result;
    }

    template<std::size_t MessagesCount, std::size_t MaxMessageSize, std::size_t MessageAlignment>
    void BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>::Reader::UpdateIndexes() {
        ++head_;
        expected_seq_ += head_ >> GetSeqRightShiftValue();
        head_ &= BoundedMulticastQueue::GetIndexMask();
    }

    template<std::size_t MessagesCount, std::size_t MaxMessageSize, std::size_t MessageAlignment>
    void BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>::Reader::Swap(
            BoundedMulticastQueue::Reader& other) noexcept {
        using std::swap;
        swap(queue_, other.queue_);
        swap(head_, other.head_);
    }

    template<std::size_t MessagesCount, std::size_t MaxMessageSize, std::size_t MessageAlignment>
    constexpr std::size_t BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>::Reader::GetSeqRightShiftValue() {
        return __builtin_ctz(GetBufferSize()) - 1;
    }


    // BoundedMulticastQueue
    template<std::size_t MessagesCount, std::size_t MaxMessageSize, std::size_t MessageAlignment>
    constexpr std::size_t BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>::GetBufferSize() {
        // assert 4
        return std::bit_ceil(MessagesCount);
    }

    template<std::size_t MessagesCount, std::size_t MaxMessageSize, std::size_t MessageAlignment>
    constexpr std::size_t BoundedMulticastQueue<MessagesCount, MaxMessageSize, MessageAlignment>::GetIndexMask() {
        return GetBufferSize() - 1;
    }

} // End of namespace concurrent::queue

#endif //LOCK_FREE_DATA_STRUCTURES_BOUNDED_SP_MC_QUEUE_H
