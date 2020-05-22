#pragma once

#include <assert.h>

#ifdef WIN32

class Event
{
public:
	Event()
	{
		mEvent = CreateEvent(nullptr, false, 0, nullptr);
	}

	~Event()
	{
		if (mEvent != nullptr)
			CloseHandle(mEvent);
	}

	void Wait(unsigned int time = 0xffffffff)
	{
		WaitForSingleObject(mEvent, time);
	}

	void Notify()
	{
		SetEvent(mEvent);
	}

private:
	HANDLE mEvent;
};

#else

class Event
{
public:
	Event() {}
	~Event() {}
};

#endif

// TODO.
__forceinline Event& GetRHIEvent()
{
	static Event sRHIEvent;
	return sRHIEvent;
}

__forceinline Event& GetRHICommandFence(int index)
{
	assert(index == 0 || index == 1);
	static Event sFence0;
	static Event sFence1;
	return index == 0 ? sFence0 : sFence1;
}
