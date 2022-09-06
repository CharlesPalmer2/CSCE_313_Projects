# <p align="center">PA3: Threading and Synchronization<p>

**Introduction**

In this programming assignment, you will be integrating multithreading to increase efficiency and improve on the runtime of PA1.

While preparing your timing report for PA1, you may have noticed that transferring over multiple data points (1K) took a long time to complete. This was also observable when using filemsg requests to transfer over raw files of extremely large sizes.

The reason behind this undesirable runtime is that we were using a single channel to transfer over each data point or chunk in a sequential manner. In PA3, we will take advantage of multithreading to implement our transfer functionality through multiple channels in a concurrent manner; this will improve on bottlenecks and make operations significantly faster.

**Tasks**

- [ ] Implement the BoundedBuffer class
  - [ ] write the BoundedBuffer::push(char*, int) function
  - [ ] write the BoundedBuffer::pop(char*, int) function
- [ ] Write functions for threads
  - [ ] patient_thread_function
  - [ ] file_thread_function
  - [ ] worker_thread_function
  - [ ] histogram_thread_function
- [ ] Create channels for all worker threads before creating threads
- [ ] Create all threads in client's main
  - [ ] patient threads and histogram threads for datapoint transfers
  - [ ] file threads for file transfers
  - [ ] worker threads for both
- [ ] Join all threads
- [ ] Close all channels

See the PA3 module on Canvas for further details and assistance.
