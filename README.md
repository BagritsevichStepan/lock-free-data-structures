# Lock Free Data Structures
The repository contains low latency lock free SPSC, MPMC Queue and Stack implementations. Also there are fast SpinLock and SeqLock implementations.

All implementations are faster than their analogs from other libraries, such as Boost.

# Links
+ [SPSCQueue](#spscqueue)
    * [Buffer](#spsc_queue_buffer)
    * [False Sharing](#spsc_queue_false_sharing)
    * [Batched Implementation](#spsc_queue_batched_impl)
    * [Benchmarks](#spsc_queue_bench)
+ [MPMCQueue](#mpmcqueue)
    * [ABA Problem](#)
    * [Reclamation Problem](#)
    * [Batched Implementation](#)
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
concurrent::queue::BoundedSPSCQueue<int, 56> q;
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
A single producer single consumer lock-free queue implementation based on a [ring buffer](https://en.wikipedia.org/wiki/Circular_buffer). This implementation is faster than `boost::lockfree::spsc`, `moodycamel::ReaderWriterQueue`, `folly::ProducerConsumerQueue` and others.

### <a name="spsc_queue_buffer"></a>Buffer
The queue is based on a ring buffer, the size of which is equal to the power of two. This allows to use bitwise operations instead of using the remainder of the division.

The basic implementation assumes that allocation will occur on the stack (for this, `std::array` is used). But you can also use heap for this along with the [Huge pages](https://wiki.debian.org/Hugepages) support.

Huge pages allow you to reduce cache misses in [TLB](https://en.wikipedia.org/wiki/Translation_lookaside_buffer). Which can significantly speed up the program.

### <a name="spsc_queue_false_sharing"></a>False Sharing


### <a name="spsc_queue_batched_impl"></a>Batched Implementation

## <a name="spsc_queue_bench"></a>Benchmarks

# MPMCQueue

## Batched Implementation


# Lock
## Fast SpinLock


## SeqLock
