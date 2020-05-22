从网上查找C++跨平台多线程的资料，主要是针对pthread和C++11中的thread对比的。

总得来说，对新手C++11的thread更加友好，但是不支持线程池。

陈硕大大说：
```
如果只在 Linux 下编程，那么 C++11 的 thread 的附加值几乎为零（我认为它过度设计了，同时损失了一些功能），你自己把 Pthreads 封装成一个简单好用的线程库只需要两三百行代码，用十几个 pthreads 函数实现 4 个 class：thread、mutex、lock_guard、condvar，而且 RAII 的好处也享受到了。
```

而网上也有c++11线程池的实现方法。
- https://www.cnblogs.com/linbc/p/5041648.html
- https://www.cnblogs.com/qxred/p/thread_pool_cpp.html

还是先系统学习c++11的thread，做出来想要的东西之后，再用pthread。

std::thread的用法不再赘述。

### 互斥量mute
``` c++
#include <iostream>
#include <thread>
#include <mutex>

void inc(std::mutex& mutex, int loop, int& counter) {
	for (int i = 0; i < loop; i++) {
		mutex.lock();
		++counter;
		mutex.unlock();
	}
}
int main() {
	std::thread threads[5];
	std::mutex mutex;
	int counter = 0;

	for (std::thread& thr : threads) {
		thr = std::thread(inc, std::ref(mutex), 1000, std::ref(counter));
	}
	for (std::thread& thr : threads) {
		thr.join();
	}

	// 输出：5000，如果inc中调用的是try_lock，则此处可能会<5000
	std::cout << counter << std::endl;

	return 0;
}
```
但是已有的对多线程渲染的知识，不是这样有用的。

- 线程1执行完操作 Signal
- 线程2 Wait，接下来继续执行任务

c++11 还提供了一些医用的类，如lock_guard, unique_lock


#### lock_guard
lock_guard利用了C++ RAII的特性，在构造函数中上锁，析构函数中解锁
```
#include <iostream>
#include <mutex>

std::mutex mutex;

void safe_thread() {
    try {
        std::lock_guard<std::mutex> _guard(mutex);
        throw std::logic_error("logic error");
    } catch (std::exception &ex) {
        std::cerr << "[caught] " << ex.what() << std::endl;
    }
}
int main() {
    safe_thread();
    // 此处仍能上锁
    mutex.lock();
    std::cout << "OK, still locked" << std::endl;
    mutex.unlock();

    return 0;
}
```

unique_lock拥有对Mutex的所有权，一但初始化了unique_lock，其就接管了该mutex, 在unique_lock结束生命周期前(析构前)，其它地方就不要再直接使用该mutex了。

### 条件变量 condition_variable
- https://en.cppreference.com/w/cpp/thread/condition_variable
- 
条件变量提供了两类操作：wait和notify。这两类操作构成了多线程同步的基础。
wait是线程的等待动作，直到其它线程将其唤醒后，才会继续往下执行。

notify有两个版本：notify_one和notify_all。

notify_one 唤醒等待的一个线程，注意只唤醒一个。
notify_all 唤醒所有等待的线程。使用该函数时应避免出现惊群效应。

### 信号量
```
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

using namespace std;

class semaphore
{
public:
    semaphore(int value = 1) :count(value) {}

    void wait()
    {
        unique_lock<mutex> lck(mtk);
        if (--count < 0)//资源不足挂起线程
            cv.wait(lck);
    }

    void signal()
    {
        unique_lock<mutex> lck(mtk);
        if (++count <= 0)//有线程挂起，唤醒一个
            cv.notify_one();
    }

private:
    int count;
    mutex mtk;
    condition_variable cv;
};


#include <mutex>
#include <condition_variable>

class Semaphore {
public:
    Semaphore (int count_ = 0)
        : count(count_) {}

    inline void notify()
    {
        std::unique_lock<std::mutex> lock(mtx);
        count++;
        cv.notify_one();
    }

    inline void wait()
    {
        std::unique_lock<std::mutex> lock(mtx);

        while(count == 0){
            cv.wait(lock);
        }
        count--;
    }

private:
    std::mutex mtx;
    std::condition_variable cv;
    int count;
};

作者：匿名用户
链接：https://www.zhihu.com/question/31555101/answer/117537944
来源：知乎
著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。
```