#pragma once

#include <functional>
#include <thread>
#include <mutex>

template <typename T>
struct ThreadPool {

    std::function<void(T&)> processor;
    std::vector<std::thread> threads;

    std::queue<T> queue;
    std::mutex mt;
    std::condition_variable cv;

    std::atomic<bool> should_stop = false;

    ThreadPool(std::function<void(T&)> processor, uint64_t thread_cnt) : processor(processor) {
        for (size_t i = 0; i < thread_cnt; ++i) {
            threads.emplace_back(&ThreadPool::worker, this);
        }
    }

    void worker() {
        std::unique_lock<std::mutex> lock(mt);
        while (true) {
            while (!should_stop && queue.empty()) {
                cv.wait(lock);
            }
            if (should_stop) {
                break;
            }
            T elem = queue.front();
            queue.pop();
            lock.unlock();
            processor(elem);
            lock.lock();
        }
    }

    void add_job(const T &val) {
        std::lock_guard<std::mutex> lk(mt);
        queue.push(val);
        cv.notify_one();
    }

    ~ThreadPool() {
        should_stop = true;
        cv.notify_all();
        for (auto &thread: threads) {
            thread.join();
        }
    }
};