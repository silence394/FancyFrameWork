#pragma once

#include <mutex>
#include <condition_variable>

class Semaphore
{
public:
	Semaphore(int count = 0)
		: mCount(count) {}

	inline void notify()
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mCount++;
		mConditionVar.notify_one();
	}

	inline void wait()
	{
		std::unique_lock<std::mutex> lock(mMutex);

		while (mCount == 0) {
			mConditionVar.wait(lock);
		}
		mCount--;
	}

private:
	int mCount;
	std::mutex mMutex;
	std::condition_variable mConditionVar;
};