# Lock Free Data Structures
The repository contains low latency lock free SPSC, MPMC Queue and Stack implementations. Also there are fast SpinLock and SeqLock implementations.

All implementations are faster than their analogs from other libraries, such as **Boost**.

# Links
+ [SPSCQueue](#spscqueue)
    * [Buffer](#spsc_queue_buffer)
    * [Cache Coherence. False Sharing](#spsc_queue_false_sharing)
    * [Batched Implementation](#spsc_queue_batched_impl)
    * [Benchmarks](#spsc_queue_bench)
+ [MPMCQueue](#mpmcqueue)
    * [ABA Problem](#)
    * [Reclamation Problem](#)
    * [Benchmarks](#)
+ [Stack](#stack)
    * [SpinLock Implementation]
    * [DCAS Lock-Free Stack]
    * [Benchmarks](#)
+ [Lock](#lock)
    * [SpinLock](#)
    * [SeqLock](#)
    * [Benchmarks](#)
+ [Benchmarking]
+ [References]
    * [Queues](#)
    * [Stacks](#)
    * [Locks](#)

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

### <a name="spsc_queue_buffer"></a>Buffer
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
| `BoundedSPSCQueue` | tmp | tmp |
| `boost::lockfree::spsc_queue` | tmp | tmp |
| `moodycamel::ReaderWriterQueue` | tmp | tmp |

# MPMCQueue

## Batched Implementation
todo
# Stack
todo

# Lock
todo
## Fast SpinLock
todo
## SeqLock
todo

# Benchmarking

