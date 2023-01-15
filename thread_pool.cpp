#include <iostream>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <vector>
#include <functional>
#include <chrono>
#include <atomic>
#include <assert.h>

class TaskQueue {
public:
    using Task = std::function<void()>;
    TaskQueue() = default;
    TaskQueue(const TaskQueue& other) = delete;
    TaskQueue& operator= (const TaskQueue& other) = delete;
    Task WaitAndPop() {
        std::unique_lock<std::mutex> lk(mtx_);
        cv_.wait(lk, [this]() { return !que_.empty(); }); //std::chrono::milliseconds(10)
        auto t = que_.front();
        if (t != nullptr) {
            que_.pop();
        }
        return t;
    }
    void AddTask(Task&& t) {
        {
            std::unique_lock<std::mutex> lk(mtx_);
            que_.push(std::forward<Task>(t));
        }
        cv_.notify_one();
    }

    std::size_t Size() {
        std::unique_lock<std::mutex> lk(mtx_);
        return que_.size();
    }
private:
    std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<Task> que_;
};

class ThreadPool {
public:
    explicit ThreadPool() : worker_cnt_(std::thread::hardware_concurrency()), index_(0) {
        try {
            for(std::size_t i = 0; i < worker_cnt_; i++) {
                task_que_.push_back(std::make_shared<TaskQueue>());
                worker_.push_back(std::thread([this, i](){
                    for(;;) {
                        auto t = task_que_[i]->WaitAndPop();
                        if (t == nullptr) {
                            continue;
                        }
                        t();
                    }
                }));
            } 
        } catch (std::exception& e) {
            std::cout << "catch exception : "<< e.what() << std::endl;
        }
    }
    
    void AddTask(TaskQueue::Task&& t) {
        std::size_t i = (index_++)%worker_cnt_;
        task_que_[i]->AddTask(std::forward<TaskQueue::Task>(t));
    }

    std::size_t Size(std::size_t i) {
        assert(i < worker_cnt_);
        return task_que_[i]->Size();
    }
    
    void Join() {
        for (auto& item : worker_) {
            item.join();
        }
    }
private:
    std::vector<std::thread> worker_;
    std::size_t worker_cnt_;
    std::vector<std::shared_ptr<TaskQueue>> task_que_;
    std::atomic<int64_t> index_;
};

int main() {
    ThreadPool pool;
    auto start = std::chrono::high_resolution_clock::now();

    std::thread t([&pool](){
        for(;;){
            std::cout << "que size ======================================= " << pool.Size(1) << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    });
    std::vector<std::thread> vec;
    for(int x = 0; x < 3; x++) {
        vec.push_back(std::thread([&pool, start, x]() {
            //std::this_thread::sleep_for(std::chrono::milliseconds(50));
            auto id = std::this_thread::get_id();
            for (int i = x*1000000; i < (x+1)*1000000; i++) {
                pool.AddTask([id, i, start](){
                    auto dur = (std::chrono::high_resolution_clock::now() - start);
                    std::cout << "from thread id = " << id << " ,i=" << i << "  ,线程id = " <<
                    std::this_thread::get_id() << " , time dur = " << std::chrono::duration_cast<std::chrono::milliseconds>(dur).count() << std::endl;
                    //std::this_thread::sleep_for(std::chrono::milliseconds(10));
                });
                //std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }));
    }

    for (auto& item : vec) {
        item.join();
    }
    pool.Join();
    t.join();   
    return 0;
}