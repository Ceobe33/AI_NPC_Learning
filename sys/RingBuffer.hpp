#pragma once
#include <vector>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <chrono>

template <typename T>
class RobustRingBuffer {
private:
    std::vector<T> buffer;
    size_t head = 0;
    size_t tail = 0;
    size_t max_size;
    bool is_full = false;

    std::mutex mtx;
    std::condition_variable not_full;
    std::condition_variable not_empty;

public:
    // RAII 构造：一次性预分配空间，防止运行期动态内存分配引发抖动
    explicit RobustRingBuffer(size_t size) : max_size(size) {
        if (max_size == 0) max_size = 16; // 防御性兜底
        buffer.resize(max_size);
    }

    ~RobustRingBuffer() = default; // 资源自动释放

    // 线程安全的阻塞写入，带超时回退机制（防永久死锁）
    bool Push(const T& item, int timeout_ms = 100) {
        std::unique_lock<std::mutex> lock(mtx);
        
        // 防御性等待：超时未等到空位则安全退出，绝不卡死系统
        if (!not_full.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this]() { return !is_full; })) {
            std::cerr << "[⚠️ 稳定性告警] 写入超时，队列已满，触发防御性熔断。\n";
            return false; 
        }

        buffer[head] = item;
        head = (head + 1) % max_size;
        is_full = (head == tail);

        not_empty.notify_one(); // 唤醒消费者
        return true;
    }

    // 线程安全的阻塞读取
    bool Pop(T& value, int timeout_ms = 100) {
        std::unique_lock<std::mutex> lock(mtx);

        if (!not_empty.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this]() { return is_full || (head != tail); })) {
            return false; // 读取超时
        }

        value = buffer[tail];
        is_full = false;
        tail = (tail + 1) % max_size;

        not_full.notify_one(); // 唤醒生产者
        return true;
    }
};
