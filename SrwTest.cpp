#include "stdafx.h"

#include "pushlock.h"

#define ASSERT(x) if (!(x)) __debugbreak();

EXTERN_C NTSYSAPI ULONG NTAPI RtlRandomEx(_Inout_ PULONG Seed);

void DoSomeTask()
{
	if (HANDLE hEvent = CreateEvent(0,0,0,0))
	{
		CloseHandle(hEvent);
	}
	YieldProcessor();
	SwitchToThread();
}

struct ThreadTestData 
{
	HANDLE hStartEvent, hStopEvent;
	SRWLOCK SRWLock = {};
	LONG numThreads = 1;

	ULONG _nTryCount = 0x10000;
	ULONG _nOwnerId = 0;
	LONG _nSC[8] = {};
	LONG _n = 0;

	void EndThread()
	{
		if (!InterlockedDecrementNoFence(&numThreads))
		{
			if (!SetEvent(hStopEvent)) __debugbreak();
		}
	}

	void DoStuff()
	{
		ULONG n = _nTryCount;
		ULONG Seed = ~GetCurrentThreadId();
		ULONG MyId = GetCurrentThreadId();

		if (WaitForSingleObject(hStartEvent, INFINITE) != WAIT_OBJECT_0) __debugbreak();

		do 
		{
			if (RtlRandomEx(&Seed) & 1)
			{
				AcquireSRWLockExclusive(&SRWLock);

				ASSERT(_nOwnerId == 0);
				ASSERT(_n == 0);
				_nOwnerId = MyId;
				_n++;

				DoSomeTask();

				_n--;
				ASSERT(_n == 0);
				ASSERT(_nOwnerId == MyId);
				_nOwnerId = 0;

				if (RtlRandomEx(&Seed) & 1)
				{
					RtlConvertSRWLockExclusiveToShared(&SRWLock);
					goto __s;
				}

				ReleaseSRWLockExclusive(&SRWLock);
			}
			else
			{
				AcquireSRWLockShared(&SRWLock);

__s:
				ASSERT(_nOwnerId == 0);
				LONG m = InterlockedIncrementNoFence(&_n);

				DoSomeTask();

				InterlockedDecrementNoFence(&_n);
				ReleaseSRWLockShared(&SRWLock);

				InterlockedIncrementNoFence(&_nSC[m - 1]);
			}

		} while (--n);

		EndThread();
	}

	static ULONG WINAPI _S_DoStuff(PVOID data)
	{
		reinterpret_cast<ThreadTestData*>(data)->DoStuff();
		return 0;
	}

	void TestInternal(PCWSTR pszCaption)
	{
		ULONG n = _countof(_nSC), m = 0;

		do 
		{
			numThreads++;

			if (HANDLE hThread = CreateThread(0, 0, _S_DoStuff, this, 0, 0))
			{
				CloseHandle(hThread);
				m++;
			}
			else
			{
				numThreads--;
			}

		} while (--n);

		EndThread();

		ULONG64 time = GetTickCount64();

		if (WAIT_OBJECT_0 != SignalObjectAndWait(hStartEvent, hStopEvent, INFINITE, FALSE))
		{
			__debugbreak();
		}

		wchar_t msg[0x100], *psz = msg;
		ULONG cch = _countof(msg);
		int len = swprintf_s(psz, cch, L"time = %I64u\r\n", GetTickCount64() - time);
		//DbgPrint("time = %I64u\n", GetTickCount64() - time);
		if (0 < len)
		{
			psz += len, cch -= len;
			do 
			{
				--m;
				if (0 < (len = swprintf_s(psz, cch, L"%02u: %08u\r\n", m + 1, _nSC[m])))
				{
					psz += len, cch -= len;
				}
				//DbgPrint("%02u: %08u\n", m + 1, _nSC[m]);
			} while (m);

			MessageBoxW(0, msg, pszCaption, MB_ICONINFORMATION);
		}
	}

	void Test(PCWSTR pszCaption)
	{
		if (hStartEvent = CreateEventW(0, TRUE, 0, 0))
		{
			if (hStopEvent = CreateEventW(0, TRUE, 0, 0))
			{
				TestInternal(pszCaption);

				CloseHandle(hStopEvent);
			}

			CloseHandle(hStartEvent);
		}
	}
};

void WINAPI ep(PCWSTR pszCaption)
{
	ULONG n = 4;
	do 
	{
		switch (n)
		{
		case 3:
			if (SwitchToPushLock())
			{
		case 2:
				pszCaption = L"[ Push ]";
			}
			break;
		case 1:
			if (SwitchToSRWLock())
			{
		case 4:
				pszCaption = L"[ SRW ]";
			}
			break;
		}
		ThreadTestData data;
		data.Test(pszCaption);
	} while (--n);

	ExitProcess(0);
}
