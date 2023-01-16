#include <iostream>
#include <boost/lockfree/queue.hpp>
#include <vector>
#include <thread>
#include <atomic>

int main() {
    using Task = int;
    boost::lockfree::queue<Task> que(512);
    std::atomic<int> at{0};
    std::vector<std::thread> readers;
    for(int i = 0; i < 4; i++) {
        readers.push_back(std::thread([&que](){
            for(;;) {
                while(!que.empty()) {
                    Task val;
                    if(que.pop(val)) {
                        std::cout << val << std::endl;
                    }
                }
            }
        }));
    }
    std::vector<std::thread> writers;
    for(int i = 0; i < 4; i++) {
        writers.push_back(std::thread([&que, &at](){
            for(int x=0; x < 30; x++) {
                que.push(at.fetch_add(1));
            }
        }));
    }
    for(auto& item : readers) {
        item.join();
    }
    for(auto& item : writers) {
        item.join();
    }
    return 0;
}