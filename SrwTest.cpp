#include "stdafx.h"

#include "pushlock.h"
#include "wlog.h"

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

	ULONG _nTryCount;
	ULONG _S_E, _R_C;
	ULONG _nOwnerId = 0;
	LONG _nSC[256] = {};
	LONG _n = 0;

	ThreadTestData(ULONG nLoops, ULONG nS_E, ULONG nR_C) : _nTryCount(nLoops), _S_E(nS_E), _R_C(nR_C)
	{
	}

	void EndThread()
	{
		if (!InterlockedDecrementNoFence(&numThreads))
		{
			if (!SetEvent(hStopEvent)) __debugbreak();
		}
	}

	void TestLock(ULONG MyId, BOOL bShared, ULONG R_C, ULONG* pSeed)
	{
		if (bShared)
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
		else
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

			if (!(RtlRandomEx(pSeed) & R_C))
			{
				RtlConvertSRWLockExclusiveToShared(&SRWLock);
				goto __s;
			}

			ReleaseSRWLockExclusive(&SRWLock);
		}
	}

	void FillStack()
	{
		memset(alloca(0x100), -1, 0x100);
	}

	void DoStuff()
	{
		ULONG n = _nTryCount;
		ULONG Seed = ~GetCurrentThreadId();
		ULONG MyId = GetCurrentThreadId();

		ULONG S_E = _S_E - 1, R_C = _R_C - 1;

		if (WaitForSingleObject(hStartEvent, INFINITE) != WAIT_OBJECT_0) __debugbreak();

		do
		{
			TestLock(MyId, RtlRandomEx(&Seed) & S_E, R_C, &Seed);
			FillStack();

		} while (--n);

		EndThread();
	}

	static ULONG WINAPI _S_DoStuff(PVOID data)
	{
		reinterpret_cast<ThreadTestData*>(data)->DoStuff();
		return 0;
	}

	void TestInternal(ULONG nThreads, PCWSTR pszCaption, WLog& log)
	{
		ULONG n = nThreads, m = 0;

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

		log(L"\r\n%s\r\n\r\ntime = %I64u\r\n", pszCaption, GetTickCount64() - time);

		do 
		{
			--m;
			log(L"%02u: %08u\r\n", m + 1, _nSC[m]);
		} while (m);
	}

	void Test(ULONG nThreads, PCWSTR pszCaption, WLog& log)
	{
		if (nThreads >= _countof(_nSC))
		{
			return ;
		}

		if (hStartEvent = CreateEventW(0, TRUE, 0, 0))
		{
			if (hStopEvent = CreateEventW(0, TRUE, 0, 0))
			{
				TestInternal(nThreads, pszCaption, log);

				CloseHandle(hStopEvent);
			}

			CloseHandle(hStartEvent);
		}
	}
};

void DoSrwTest(WLog& log, ULONG nThreads, ULONG nLoops, ULONG nS_E, ULONG nR_C, PCWSTR pszCaption)
{
	ThreadTestData data(nLoops, nS_E, nR_C);
	data.Test(nThreads, pszCaption, log);
}

BOOL ParseCmdLine(ULONG* pnThreads, ULONG* pnLoops, ULONG* pnS_E, ULONG* pnR_C);
BOOL CheckValues(WLog& log, ULONG nThreads, ULONG nLoops, ULONG nS_E, ULONG nR_C);

void DoSrwTest(WLog& log)
{
	ULONG nThreads = 8, nLoops = 0x10000, nS_E = 8, nR_C = 4;

	if (ParseCmdLine(&nThreads, &nLoops, &nS_E, &nR_C) &&
		CheckValues(log, nThreads, nLoops, nS_E, nR_C))
	{
		PCWSTR pszCaption = 0;
		ULONG n = 4;

		log(L"Threads=%u Loops=%u shared/exlusive=%u release/convert=%u\r\r\r\n", nThreads , nLoops, nS_E , nR_C);
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

			DoSrwTest(log, nThreads, nLoops, nS_E, nR_C, pszCaption);

		} while (--n);
	}
}
