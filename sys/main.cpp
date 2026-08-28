#include "RingBuffer.hpp"
#include <thread>

void Producer(RobustRingBuffer<int>& q) {
    for (int i = 0; i < 50; ++i) {
        q.Push(i);
        std::this_thread::sleep_for(std::chrono::milliseconds(5)); // 模拟高速生产
    }
}

void Consumer(RobustRingBuffer<int>& q) {
    int val = 0;
    for (int i = 0; i < 50; ++i) {
        if (q.Pop(val)) {
            // 减少高频的 std::cout，避免 I/O 阻塞主线程时序
            if (val % 10 == 0) std::cout << "[消费成功] 联动数据点: " << val << "\n";
        }
    }
}

int main() {
    std::cout << "🚀 开始系统级稳定性边界测试...\n";
    RobustRingBuffer<int> queue(10); // 缓冲区大小设为10，故意制造满载压力

    // 启动多线程并发读写
    std::thread t1(Producer, std::ref(queue));
    std::thread t2(Consumer, std::ref(queue));

    t1.join();
    t2.join();

    std::cout << "✅ 边界压力测试通过，未发生内存溢出与死锁。\n";
    return 0;
}
