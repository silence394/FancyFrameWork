#pragma once

// TODO.
__forceinline HANDLE GetRHIEvent()
{
	static HANDLE sRHIEvent = nullptr;
	if (sRHIEvent == nullptr)
	{
		sRHIEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	}

	return sRHIEvent;
}

__forceinline HANDLE GetRHICommandFence(int index)
{
	static HANDLE sFence0 = nullptr;
	if (sFence0 == nullptr)
	{
		sFence0 = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		SetEvent(sFence0);
	}

	static HANDLE sFence1 = nullptr;
	if (sFence1 == nullptr)
	{
		sFence1 = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		SetEvent(sFence1);
	}

	if (index != 0 && index != 1)
		return nullptr;

	return index == 0 ? sFence0 : sFence1;
}
