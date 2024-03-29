# Lock Free Data Structures
The repository contains low latency lock free SPSC, SPMC, MPMC Queue and Stack implementations. Also there are fast SpinLock and SeqLock implementations.

All implementations are faster than their analogs from other libraries, such as **Folly**, or **Boost**.

# Links
+ [SPSCQueue](#spscqueue)
    * [Buffer. Huge Pages](#spsc_queue_buffer)
    * [Cache Coherence. False Sharing](#spsc_queue_false_sharing)
    * [Batched Implementation](#spsc_queue_batched_impl)
    * [Benchmarks](#spsc_queue_bench)
+ [Multicast SPMCQueue](#spmc_queue)
    * [SeqLock Approach](#spmc_queue_seqlock)
    * [Reader Interface](#spmc_queue_reader)
    * [Benchmarks](#spmc_queue_bench)
+ [MPMCQueue](#mpmcqueue)
    * [Generations Approach](#mpmc_queue_generation)
    * [Benchmarks](#mpmc_queue_bench)
+ [Stack](#stack)
    * [Reclamation Problem](#stack_reclamation)
    * [ABA Problem](#stack_aba)
    * [DCAS Lock-Free Stack](#stack_lock_free)
    * [SpinLock Implementation](#stack_spin_lock)
    * [Benchmarks](#stack_bench)
+ [Lock](#lock)
    * [Fast SpinLock](#lock_spinlock)
    * [SeqLock](#lock_seqlock)
         * [C++ Memory Model Problem](#lock_memory_model)
         * [Atomic Memcpy](#lock_atomic_memcpy)
    * [Benchmarks](#lock_bench)
+ [Benchmarking](#benchmarking)
    * [Tuning](#bench_tuning)
+ [References](#references)

# SPSCQueue
```cpp
const size_t capacity = 70;
concurrent::queue::BoundedSPSCQueue<int, capacity> q;
auto consumer = std::thread([&q]() {
  for (int i = 0; i < 100; i++) {
    while (!q.Dequeue());
  }
});
auto producer = std::thread([&q]() {
  for (int i = 0; i < 100; i++) {
    while (!q.Enqueue(i));
  }
});
```
A single producer single consumer lock-free queue implementation based on a [ring buffer](https://en.wikipedia.org/wiki/Circular_buffer). This implementation is faster than [`boost::lockfree::spsc_queue`](https://www.boost.org/doc/libs/1_60_0/boost/lockfree/spsc_queue.hpp), [`moodycamel::ReaderWriterQueue`](https://github.com/cameron314/readerwriterqueue), [`folly::ProducerConsumerQueue`](https://github.com/facebook/folly/blob/main/folly/ProducerConsumerQueue.h) and others.

### <a name="spsc_queue_buffer"></a>Buffer. Huge Pages
The queue is based on a ring buffer, the size of which is equal to the power of two. This allows to use bitwise operations instead of using the remainder of the division.

The basic implementation assumes that allocation will occur on the stack (for this, `std::array` is used). But you can also use heap allocation for this along with the [Huge pages](https://wiki.debian.org/Hugepages) support.

Huge pages allow you to reduce cache misses in [TLB](https://en.wikipedia.org/wiki/Translation_lookaside_buffer). Which can significantly speed up the program.

For this, you can use `concurrent::allocator::HugePageAllocator`. But not all platforms support its implementation.

### <a name="spsc_queue_false_sharing"></a>Cache Coherence. False Sharing
[False sharing](https://en.wikipedia.org/wiki/False_sharing) is a known problem for concurrent data structures. To avoid this problem, paddings are used between the variables. See [`utils/cache_line.h`](https://github.com/BagritsevichStepan/lock-free-data-structures/blob/main/utils/cache_line.h).

Besides, aligning variables to the cache line helps to avoid false sharing. You can define your cache line size using `CACHE_LINE_SIZE` compiler flag (by default it is 64 bytes). See [`concurrent::cache::kCacheLineSize`](https://github.com/BagritsevichStepan/lock-free-data-structures/blob/main/utils/cache_line.h).

In addition, the producer uses `head_` to make sure that the buffer is not full. Instead of loading the atomic `head_` every time, we can cache the `head_` value for producer (see `cached_head_`).

On the other hand, consumer uses atomic `tail_` to make sure that the buffer is not empty. We can also use cached `tail_` value for consumer (see `cached_tail_`).

In total, three approaches are used to reduce the amount of coherency traffic:
1. Cached `head_` and `tail_` values
2. Paddings between shared variables
3. Alignment of the shared variables using `alignas`

### <a name="spsc_queue_batched_impl"></a>Batched Implementation
Batched push and pop operations can reduce the number of atomic indices needs to be loaded and updated. Sometimes it can speed up the program.

See [`concurrent::queue::BatchedBoundedSPSCQueue`](https://github.com/BagritsevichStepan/lock-free-data-structures/blob/main/queue/batched_bounded_sp_sc_queue.h).

## <a name="spsc_queue_bench"></a>Benchmarks
Benchmark measures throughput between 2 threads for a queue of `int` items.

To get full information on how the measurements were taking, please see [Benchmarking](#benchmarking) chapter.

| Queue | Throughput (ops/ms) | Latency RTT (ns) |
| --- | --- | --- |
| `concurrent::queue::BoundedSPSCQueue` | 61250 | 158 |
| `boost::lockfree::spsc_queue` | 11345 | 449 |
| `folly::ProducerConsumerQueue` | 14614 | 321 |
| `moodycamel::ReaderWriterQueue` | 21815 | 273 |

# <a name="spmc_queue"></a>Multicast SPMCQueue
```cpp
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
   }
});
```
A single producer multi-consumer lock-free multicast queue implementation based on a [ring buffer](https://en.wikipedia.org/wiki/Circular_buffer).

The queue is used in multicast mode - every message written by the producer is read by all consumers. So it can be easily used for inter-thread or inter-process communication. For example, in HFT it is used to share data between market data receiver and trading strategies (OMS). 

It supports only **trivially copyable** and **trivially destructible** types.

### <a name="spmc_queue_seqlock"></a>SeqLock Approach
A key feature of this queue is that the writer is never blocked by readers, it continuously writes data.

This is achieved using an approach similar to [seqlock](#lock_seqlock):

Before writing, the writer increments the counter, which is called the sequence number, by 1 (it becomes odd), and after writing, increases the counter by 1 (the counter becomes even). Readers check at the time that the counter values have not changed before and after reading (at the same time, the counter must be even, i.e. there is no writing process).

### <a name="spmc_queue_reader"></a>Reader Interface
```cpp
// < 0 - The data was not updated. The reader must wait
// == 0 - The expected data version was read
// > 0 - The data was overwritten several times. The reader is late
int32_t TryRead(Message& message);

// true - The message was read
// false - The data was overwritten several times. The reader is late
bool Read(Message& message);

// The function must be called if the TryRead() function returned 0
void UpdateIndexes();
```

The `Read` method waits for the message to be written. If the reading fails, the `PAUSE` instruction is called (see [concurrent::wait::Wait](https://github.com/BagritsevichStepan/lock-free-data-structures/blob/main/utils/wait.h)). If the reader has missed a message in the cell (the counter has increased by more than 2), false is returned. In other words, it means that the reader is late and, for example, can be stopped.

The `TryRead` method is non-waiting version of `Read` method. It returns 3 types of values:
1. `returned value < 0` - The data was not updated. The reader must wait
2. `returned value == 0` - The expected data version was read. Please, call `UpdateIndexes` to read next data
3. `returned value > 0` - The data was overwritten several times. The reader is late

## <a name="spmc_queue_bench"></a>Benchmarks
Benchmark measures throughput between 1 writers and 3 readers for a queue of messages with one `int` variable.

The mutlicast queue was compared with `SPSCBasedSPMCQueue`. This queue is based on n [`concurrent::queue::BoundedSPSCQueue`](#spscqueue)'s, where `n` is the number of readers. Thus, the writer writes in a separate queue for each reader.

As a result, the multicast queue is two times faster than [`concurrent::queue::SPSCBasedSPMCQueue`](https://github.com/BagritsevichStepan/lock-free-data-structures/blob/main/benchmarks/benchmark_sp_mc_queues.cpp).

To get full information on how the measurements were taking, please see [Benchmarking](#benchmarking) chapter.

| Queue | Throughput (ops/ms) |
| --- | --- |
| `concurrent::queue::BoundedMulticastQueue` | 4843 |
| `concurrent::queue::SPSCBasedSPMCQueue` | 2279 |

# MPMCQueue
```cpp
const size_t capacity = 400;
concurrent::queue::BoundedMPMCQueue<int, capacity> q;
for (int t = 0; t < 3; t++) {
   consumers.emplace_back([&q]() {
      int result = 0;
      for (int i = 0; i < 100; i++) {
         q.Dequeue(result);
      }
   });
   producers.emplace_back([&q]() {
      for (int i = 0; i < 100; i++) {
         q.Emplace(i);
      }
   });
}
```
A multi-producer multi-consumer lock-free queue implementation based on a [ring buffer](https://en.wikipedia.org/wiki/Circular_buffer).

Note that some optimizations from `concurrent::queue::BoundedSPSCQueue` are used here, so please familiarize yourself with them first
([Buffer. Huge Pages](#spsc_queue_buffer), [Cache Coherence. False Sharing](#spsc_queue_false_sharing), [Batched Implementation](#spsc_queue_batched_impl)).

MPMCQueue implementation should be resistant to the [ABA](#stack_aba) and the [Reclamation Problem](#stack_reclamation).

### <a name="mpmc_queue_generation"></a>Generations Approach
<img src="MPMCQueue.jpg" alt="MPMCQueue" width="70%"/>

Generation approach is used to solve the [ABA](#stack_aba) and the [Reclamation Problem](#stack_reclamation). For this, each buffer element stores a version (or generation) with the data.

An even generation value means that the data is missing. Odd, on the contrary, that the cell is occupied.

Thus, each time we change the cell, we must increase the generation by one.

## <a name="mpmc_queue_bench"></a>Benchmarks
Comming soon...

# Stack
Fast concurrent stack implementations.

Concurrent stack implementation should be resistant to the [ABA](#stack_aba) and the [Reclamation Problem](#stack_reclamation).

Note that the blocking stack ([`concurrent::stack::UnboundedSpinLockedStack`](#stack_spin_lock)) works faster than lock-free version ([`concurrent::stack::UnboundedLockFreeStack`](#stack_lock_free)), so it's better to use it.

### <a name="stack_reclamation"></a>Reclamation Problem
The [Reclamation Problem](https://arxiv.org/pdf/1712.01044.pdf) is a typical problem for concurrent data structures. For concurrent stack, the problem is in the `Pop` method:
```cpp
bool Pop(T& data) {
   Node* node = head.load();
   do {
      if (!node) return false;
   } while (!head.compare_exchange_weak(node, node->next));
   ...
   delete node;
   ...
}
```
While one thread executed `if (!node) return false`, the second deleted this node. In the line `while (!head.compare_exchange_weak(node, node->next))` we will use the deleted data. According to the C++ standard, this is undefined behavior.

### <a name="stack_aba"></a>ABA Problem
The [ABA problem](https://en.wikipedia.org/wiki/ABA_problem) is very similar to the reclamation problem. The problem is again in the `Pop` method.

While one thread executed `if (!node) return false`, the second deleted this node. The third thread executed `Pop` method (so, it deleted `node->next`) and then pushed `node` again. Thus, in the first thread `head.compare_exchange_weak(node, node->next)` will return true. As a result, we will get an invalid stack.

## <a name="stack_lock_free"></a>DCAS Lock-Free Stack
```cpp
concurrent::stack::UnboundedLockFreeStack<int> stack;
auto consumer = std::thread([&stack]() {
   int result = 0;
   for (int i = 0; i < 100; i++) {
      while (!stack.Pop(result));
   }
});
auto producer = std::thread([&stack]() {
   for (int i = 0; i < 100; i++) {
      stack.Push(i);
   }
});
```
Lock free stack implementation based on [lock free atomic shared pointer](https://www.youtube.com/watch?v=gTpubZ8N0no) implementation.

It is a list of nodes, whose references are stored as `SharedPtr`. The reference to the head is `AtomicSharedPtr`.

Thus, with this approach, we solve the [ABA](#stack_aba) and the [Reclamation Problem](#stack_reclamation).

`UnboundedLockFreeStack` uses simple hack. Inside 64-bit pointer there is 16-bit reference counter. You can do it as long as your addresses can fit in 48-bit (this is true on most platforms).

## <a name="stack_spin_lock"></a>SpinLock Implementation
```cpp
concurrent::stack::UnboundedSpinLockedStack<int> stack;
auto consumer = std::thread([&stack]() {
   int result = 0;
   for (int i = 0; i < 100; i++) {
      while (!stack.Pop(result));
   }
});
auto producer = std::thread([&stack]() {
   for (int i = 0; i < 100; i++) {
      stack.Push(i);
   }
});
```
Blocked stack implementation.

It uses the basic `std::stack` implementation and fast [`SpinLock`](#lock_spinlock) implementation.

Also, you can pass your own single-threaded stack and lock implementations. For this, use `concurrent::stack::UnboundedLockedStack`.

For example, [`concurrent::stack::UnboundedMutexLockedStack`](https://github.com/BagritsevichStepan/lock-free-data-structures/blob/main/stack/unbounded_locked_stack.h) uses `std::mutex` instead of `SpinLock`.

## <a name="stack_bench"></a>Benchmarks
Benchmark measures throughput between 2 threads for a stack of `int` items.

To get full information on how the measurements were taking, please see [Benchmarking](#benchmarking) chapter.

| Queue | Throughput (ops/ms)
| --- | --- |
| `concurrent::stack::UnboundedSpinLockedStack` | 10019 |
| `concurrent::stack::UnboundedLockFreeStack` | 759 |
| `boost::lockfree::stack` | 4912 |
| `LFStructs::LFStack` | 1448 |
| `concurrent::stack::UnboundedMutexLockedStack` | 7305 |

# Lock
Several lock implementations that are faster than `std::mutex`.

Both locks using `alignas` to the cache line to avoid [split lock](https://lwn.net/Articles/790464/). You can define your cache line size using `CACHE_LINE_SIZE` compiler flag (by default it is 64 bytes). See [`concurrent::cache::kCacheLineSize`](https://github.com/BagritsevichStepan/lock-free-data-structures/blob/main/utils/cache_line.h).

## <a name="lock_spinlock"></a>Fast SpinLock
```cpp
concurrent::lock::SpinLock spin_lock;
auto writer = std::thread([&shared_data, &spin_lock]() {
   spin_lock.Lock();
   *shared_data = 15;
   spin_lock.Unlock();
});
auto reader = std::thread([&shared_data, &spin_lock]() {
   spin_lock.Lock();
   std::cout << *shared_data << std::endl;
   spin_lock.Unlock();
});
```
Faster implementation of `SpinLock` based on [test and test-and-set (TTAS)](https://en.wikipedia.org/wiki/Test_and_test-and-set) approach.

Regarding to the coherency protocol ([MESI](https://en.wikipedia.org/wiki/MESI_protocol), [MOESI](https://en.wikipedia.org/wiki/MOESI_protocol), [MESIF](https://en.wikipedia.org/wiki/MESIF_protocol)), reading is much faster than writing. This is why `SpinLock` tries to load the value of the atomic flag more often than to exchange it.

In addition, `SpinLock` uses `PAUSE` instruction when the loaded flag is locked. It is needed to reduce power usage and contention on the load-store units. See [concurrent::wait::Wait](https://github.com/BagritsevichStepan/lock-free-data-structures/blob/main/utils/wait.h).

## <a name="lock_seqlock"></a>SeqLock
```cpp
concurrent::lock::SeqLockAtomic<int> shared_data{5};
auto writer = std::thread([&shared_data]() {
   shared_data.Store(15);
});
auto reader = std::thread([&shared_data]() {
   std::this_thread::sleep_for(1ms);
   std::cout << shared_data.Load() << std::endl;
});
```
Implementation of the [SeqLock](https://en.wikipedia.org/wiki/Seqlock). 

`SeqLock` is an alternative to the [readers–writer lock](https://en.wikipedia.org/wiki/Readers–writer_lock), which avoids the problem of writer starvation. It never blocks the reader, so it works fast in the programs with a large number of readers.

Please use `concurrent::lock::SeqLockAtomic` to load and store your shared data. Internally, it uses `concurrent::lock::SeqLock` to synchronize reads and writes. 

Also, `concurrent::lock::SeqLockAtomic` supports only **trivially copyable** types.

### <a name="lock_memory_model"></a>C++ Memory Model Problem
The main problem is to ensure that there is [happens before](https://en.wikipedia.org/wiki/Happened-before) relation between read and write operations.

Read operation (pseudocode):
```cpp
do {
   seq0 = atomic_load(seq, memory_order_acquire);
   data_copy = atomic_load(data, memory_order_relaxed);
   seq1 = atomic_load(seq, memory_order_release);
} while (IsLocked(seq0) || seq0 != seq1);
```
Write operation (pseudocode):
```cpp
lock(seq, memory_order_acquire);
atomic_store(data, desired_data, memory_order_relaxed);
unlock(seq, memory_order_release);
```

The main problem is in the read operation, in the line `seq1 = atomic_load(seq, memory_order_release)`. C++ doesn't support atomic loading operation with `std::memory_order_release` memory order.

To solve this problem, we can use `std::atomic_thread_fence`. The final solution looks like this:

Read operation:
```cpp
do {
   seq0 = seq_.load(std::memory_order_acquire);
   atomic_memcpy_load(&data_copy, &data_, sizeof(data_));
   std::atomic_thread_fence(std::memory_order_acquire);
   seq1 = seq_.load(std::memory_order_relaxed);
} while (IsLocked(seq0) || seq0 != seq1);
```
Write operation:
```cpp
seq_lock_.Lock(std::memory_order_acquire);
std::atomic_thread_fence(std::memory_order_release);
atomic_memcpy_store(&data_, &desired_data, sizeof(data_));
seq_lock_.Unlock(std::memory_order_release);
```

### <a name="lock_atomic_memcpy"></a>Atomic Memcpy
The other problem is that, `memcpy_load` and `memcpy_store` operations can be performed simultaneously in different threads. If they are not synchronized, then according to the C++ standard, this is undefined behavior.

So we need to implement `atomic_memcpy_load` and `atomic_memcpy_store` operations.

## <a name="lock_bench"></a>Benchmarks
Comming soon...

# Benchmarking
Comming soon...

## <a name="bench_tuning"></a>Tuning
Comming soon...

# References
Queues:
* [Bounded MPMC queue](https://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue)
* [Toward high-throughput algorithms on many-core architectures](https://dl.acm.org/doi/10.1145/2086696.2086728)
* [The Baskets Queue](http://people.csail.mit.edu/shanir/publications/Baskets%20Queue.pdf)

Stacks:
* [Lock-free Atomic Shared Pointers Without a Split Reference Count?](https://www.youtube.com/watch?app=desktop&v=lNPZV9Iqo3U)
* [A Scalable Lock-free Stack Algorithm](https://people.csail.mit.edu/shanir/publications/Lock_Free.pdf)

Locks:
* [Using locks in real-time audio processing, safely](https://timur.audio/using-locks-in-real-time-audio-processing-safely)
* [Can Seqlocks Get Along With Programming Language Memory Models?](https://www.hpl.hp.com/techreports/2012/HPL-2012-68.pdf)
* [Byte-wise atomic memcpy](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p1478r7.html)




