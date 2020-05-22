#pragma once

#include "Semaphore.h"

template<typename T>
class LockFreeQueue
{
public:
	LockFreeQueue()
		: mIn(0), mOut(0) {}

	void Push(T value)
	{
		int next = (mIn + 1) % MAX_SIZE;
		while (next == mOut)
		{
			mWriteSem.wait();
		}

		mContent[mIn] = value;
		mIn = next;

		mReadSem.notify();
	}

	T Pop()
	{
		while (mOut == mIn)
		{
			mReadSem.wait();
		}

		int temp = mOut;

		mOut = (mOut + 1) % MAX_SIZE;
		mWriteSem.notify();

		return mContent[temp];;
	}

private:
	const static int MAX_SIZE = 1024;
	T mContent[MAX_SIZE];

	int mIn;
	int mOut;
	Semaphore mReadSem;
	Semaphore mWriteSem;
};