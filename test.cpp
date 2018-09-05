// crt_17.00.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

extern "C" 
{
	NTSYSAPI
		ULONG
		__cdecl
		DbgPrint (
		_In_z_ _Printf_format_string_ PCSTR Format,
		...
		);
	NTSYSAPI ULONG NTAPI RtlRandomEx ( _Inout_ PULONG Seed );
};

#define STATIC_ASTRING(name, str) static const CHAR name[] = str
#define TRI_ASSERT(x) if (!(x)) __debugbreak();
#define ASSERT(x) if (!(x)) __debugbreak();

class FairReadWriteLock {
private:
	/// @brief an object that can be notified and that is linked with
	/// its followers in a singly linked list
	struct Notifiable {
		Notifiable(std::condition_variable* cond) 
			: cond(cond), next(nullptr) {}

		/// @brief condition variable, used to notify the object
		std::condition_variable* cond;

		/// @brief next following object in singly linked list
		Notifiable* next;
	};


	/// @brief a simple queue for readers or writers
	class Queue {
	public:
		/// @brief creates an empty queue
		Queue() 
			: _start(nullptr), _end(nullptr) {}

		/// @brief whether or not the queue contains elements
		bool empty() const { return _start == nullptr; }

		/// @brief wake up the queue head if the queue is non-empty
		/// returns true if the queue head was woken up, false otherwise
		bool notifyOne() {
			if (empty()) {
				// queue is empty, nothing to do
				return false;
			}
			// wake up queue head
			_start->cond->notify_one();
			return true;
		}

		/// @brief wake up all waiters in the queue head if the queue is non-empty
		/// returns true if at least one waiter was woken up, false otherwise
		bool notifyAll() {
			if (empty()) {
				// queue is empty, nothing to do
				return false;
			}
			// wake up queue head
			_start->cond->notify_all();
			return true;
		}

		/// @brief adds the Notifiable to the queue
		void add(Notifiable* notifiable) noexcept {
			if (empty()) {
				// queue is empty
				TRI_ASSERT(_end == nullptr);
				_start = notifiable;
			} else {
				// queue is not empty
				TRI_ASSERT(_end != nullptr);
				_end->next = notifiable;
			}
			_end = notifiable;
		}

		/// @brief removes a notifiable from the queue
		/// asserts that the notifiable exists in the queue
		void remove(Notifiable* notifiable) noexcept {
			// element is at start of the queue
			if (_start == notifiable) {
				_start = _start->next;
				if (_start == nullptr) {
					// queue is now empty
					_end = nullptr;
					TRI_ASSERT(empty());
				}
				return;
			}

			// element was not at start of queue. now find it
			Notifiable* n = _start;
			while (n != nullptr) {
				if (n->next == notifiable) {
					n->next = notifiable->next;
					if (n->next == nullptr) {
						_end = n;
					}
					return;
				}
				n = n->next;
			}
			TRI_ASSERT(false);
		}

	private:
		/// @brief queue head
		Notifiable* _start;

		/// @brief queue tail
		Notifiable* _end;
	};

	/// @brief type for next preferred operation phase
	enum class Phase : int32_t {
		READ,
		WRITE
	};

	typedef std::chrono::duration<double> duration_t;
	typedef std::chrono::time_point<std::chrono::steady_clock, duration_t> timepoint_t;

private:
	/// @brief mutex protecting the entire data structure
	std::mutex _mutex;

	/// @brief currently queued write operations that will be notified eventually
	Queue _writeQueue;

	/// @brief currently queued read operations that will be notified eventually
	Queue _readQueue;

	/// @brief condition variable, used to notify all readers
	std::condition_variable _readCondition;

	// @brief who is currently holding the lock:
	//    -1 = a writer got the lock, 
	//     0 = noone got the lock
	//   > 0 = # readers got the lock
	int32_t _whoEntered; 

	/// @brief next phase the lock will grant access to 
	/// when there are both readers and writers queued
	Phase _nextPreferredPhase;

 public:
	 FairReadWriteLock(FairReadWriteLock const&) = delete;
	 FairReadWriteLock& operator=(FairReadWriteLock const&) = delete;

	 /// @brief creates a new lock instance
	 FairReadWriteLock() 
		 : _whoEntered(0), 
		 _nextPreferredPhase(Phase::READ) {}
	 /// @brief locks for writing
	 void writeLock() {
		 std::unique_lock<std::mutex> guard(_mutex);

		 if (_whoEntered != 0 ||
			 (!_readQueue.empty() && _nextPreferredPhase == Phase::READ)) {
				 // someone has acquired the lock already, or noone has acquired the lock,
				 // but it is a reader's turn now

				 // put a Notifiable object into the write queue, that can be woken up whenever
				 // it is its turn 
				 // each writer gets its own condition variable
				 std::condition_variable cond;
				 Notifiable notifiable(&cond);
				 _writeQueue.add(&notifiable);

				 cond.wait(guard, [this]() -> bool { 
					 return _whoEntered == 0 && (_readQueue.empty() || _nextPreferredPhase == Phase::WRITE); 
				 });

				 // clean up write queue
				 _writeQueue.remove(&notifiable);
		 }

		 // successfully acquired the write-lock
		 TRI_ASSERT(_whoEntered == 0);
		 _whoEntered = -1; // a writer
		 _nextPreferredPhase = Phase::READ;
	 }

	 /// @brief releases the write-lock
	 void unlockWrite() {
		 std::unique_lock<std::mutex> guard(_mutex);

		 // when we get here, exactly one writer must have acquired the lock
		 TRI_ASSERT(_whoEntered == - 1);
		 _whoEntered = 0;

		 // wake up potential other waiters
		 if (_nextPreferredPhase == Phase::READ) {
			 if (!_readQueue.notifyAll()) {
				 _writeQueue.notifyOne();
			 }
		 } else if (_nextPreferredPhase == Phase::WRITE) {
			 if (!_writeQueue.notifyOne()) {
				 _readQueue.notifyAll();
			 }
		 }
	 }

	 /// @brief locks for reading
	 void readLock() {
		 std::unique_lock<std::mutex> guard(_mutex);

		 if (_whoEntered == -1 || 
			 (!_writeQueue.empty() && _nextPreferredPhase == Phase::WRITE)) {
				 // a writer has acquired the lock already, or no writer has acquired the lock yet,
				 // but it is a writer's turn now

				 // put a Notifiable object into the write queue, that can be woken up whenever
				 // it is its turn 
				 // all readers share the same condition variable
				 Notifiable notifiable(&_readCondition);
				 _readQueue.add(&notifiable);

				 _readCondition.wait(guard, [this]() -> bool { 
					 return _whoEntered >= 0 && (_writeQueue.empty() || _nextPreferredPhase == Phase::READ); 
				 });

				 // clean up read queue
				 _readQueue.remove(&notifiable);
		 }

		 // successfully acquired the read-lock
		 TRI_ASSERT(_whoEntered >= 0);
		 ++_whoEntered;
		 _nextPreferredPhase = Phase::WRITE;
	 }

	 /// @brief releases the read-lock
	 void unlockRead() {
		 std::unique_lock<std::mutex> guard(_mutex);

		 // when we get here, there must have been at least one reader having
		 // acquired the lock, but no writers    
		 TRI_ASSERT(_whoEntered > 0);
		 --_whoEntered;

		 // wake up potential other waiters
		 if (_nextPreferredPhase == Phase::READ) {
			 if (!_readQueue.notifyAll()) {
				 _writeQueue.notifyOne();
			 }
		 } else if (_nextPreferredPhase == Phase::WRITE) {
			 if (!_writeQueue.notifyOne()) {
				 _readQueue.notifyAll();
			 }
		 }
	 }

	 /// @brief releases the lock, either read or write
	 void unlock() {
		 std::unique_lock<std::mutex> guard(_mutex);

		 if (_whoEntered == - 1) {
			 // when we get here, exactly one writer must have acquired the lock
			 _whoEntered = 0;
		 } else {
			 // when we get here, there must have been at least one reader having
			 // acquired the lock, but no writers    
			 TRI_ASSERT(_whoEntered > 0);
			 --_whoEntered;
		 }

		 // wake up potential other waiters
		 if (_nextPreferredPhase == Phase::READ) {
			 if (!_readQueue.notifyAll()) {
				 _writeQueue.notifyOne();
			 }
		 } else if (_nextPreferredPhase == Phase::WRITE) {
			 if (!_writeQueue.notifyOne()) {
				 _readQueue.notifyAll();
			 }
		 }
	 }
};

#pragma warning(disable : 4706)

#include "pushlock.h"

struct ThreadData : FairReadWriteLock, CPushLock
{
	SRWLOCK _SRWLock;
	HANDLE _hStartEvent, _hStopEvent;
	ULONG _nTryCount;
	ULONG _nOwnerId;
	LONG _nTaskCount;
	LONG _nSC[8];
	LONG _n;

	ThreadData(ULONG nTryCount)
	{
		_hStartEvent = 0, _hStopEvent = 0;
		_SRWLock.Ptr = 0;
		_nTryCount = nTryCount;
		RtlZeroMemory(_nSC, sizeof(_nSC));
		_n = 0;
		_nOwnerId = 0;
	}

	~ThreadData()
	{
		if (_hStopEvent) CloseHandle(_hStopEvent);
		if (_hStartEvent) CloseHandle(_hStartEvent);
	}

	ULONG Start(LONG nTaskCount)
	{
		_nTaskCount = nTaskCount;
		return SetEvent(_hStartEvent) ? NOERROR : GetLastError();
	}

	void Stop()
	{
		if (!InterlockedDecrementNoFence(&_nTaskCount))
		{
			SetEvent(_hStopEvent);
		}
	}

	ULONG Create()
	{
		return (_hStartEvent = CreateEvent(0, TRUE, FALSE, 0)) && 
			(_hStopEvent = CreateEvent(0, TRUE, FALSE, 0)) ? NOERROR : GetLastError();
	}

	void WaitStart()
	{
		if (WaitForSingleObject(_hStartEvent, INFINITE) != WAIT_OBJECT_0) __debugbreak();
	}

	void WaitStop()
	{
		if (WaitForSingleObject(_hStopEvent, INFINITE) != WAIT_OBJECT_0) __debugbreak();
	}
};

//#define _USE_SRW_
//#define _USE_FAIR_
#define _USE_PUSH_

#ifdef _USE_SRW_
#define AcquireLockExclusive(p)	AcquireSRWLockExclusive(&p->_SRWLock)
#define ReleaseLockExclusive(p)	ReleaseSRWLockExclusive(&p->_SRWLock)
#define AcquireLockShared(p)	AcquireSRWLockShared(&p->_SRWLock)
#define ReleaseLockShared(p)	ReleaseSRWLockShared(&p->_SRWLock)
#elif defined(_USE_FAIR_)
#define AcquireLockExclusive(p)	p->writeLock();
#define ReleaseLockExclusive(p)	p->unlockWrite();
#define AcquireLockShared(p)	p->readLock()
#define ReleaseLockShared(p)	p->unlockRead()
#elif defined(_USE_PUSH_)
#define AcquireLockExclusive(p)	p->AcquireExclusive();
#define ReleaseLockExclusive(p)	p->ReleaseExclusive();
#define AcquireLockShared(p)	p->AcquireShared()
#define ReleaseLockShared(p)	p->ReleaseShared()
#else
#error
#endif

//#define _TEST_EXCLUSIVE_TO_SHARED_

void DoSomeTask()
{
	if (HANDLE hEvent = CreateEvent(0,0,0,0))
	{
		CloseHandle(hEvent);
	}
	YieldProcessor();
	SwitchToThread();
}

ULONG WINAPI TT00(ThreadData* pData)
{
	ULONG n = pData->_nTryCount;

	ULONG Seed = ~GetCurrentThreadId();

	pData->WaitStart();

	ULONG MyId = GetCurrentThreadId();

	do 
	{
		if (RtlRandomEx(&Seed) & 1)
		{
			AcquireLockExclusive(pData);

			ASSERT(pData->_nOwnerId == 0);
			ASSERT(pData->_n == 0);
			pData->_nOwnerId = MyId;
			pData->_n++;

			DoSomeTask();

			pData->_n--;
			ASSERT(pData->_n == 0);
			ASSERT(pData->_nOwnerId == MyId);
			pData->_nOwnerId = 0;

#if defined(_TEST_EXCLUSIVE_TO_SHARED_) && defined(_USE_PUSH_)
			if (RtlRandomEx(&Seed) & 1)
			{
				pData->ConvertExclusiveToShared();
				goto __s;
			}
#endif
			ReleaseLockExclusive(pData);
		}
		else
		{
			AcquireLockShared(pData);

#if defined(_TEST_EXCLUSIVE_TO_SHARED_) && defined(_USE_PUSH_)
__s:
#endif
			ASSERT(pData->_nOwnerId == 0);
			LONG m = InterlockedIncrementNoFence(&pData->_n);

			DoSomeTask();

			InterlockedDecrementNoFence(&pData->_n);
			ReleaseLockShared(pData);

			InterlockedIncrementNoFence(&pData->_nSC[m - 1]);
		}

	} while (--n);

	pData->Stop();

	return __LINE__;
}

void LockTest()
{
	ThreadData td(0x10000);

	if (!td.Create())
	{
		HANDLE hThreads[8], *ph = hThreads;
		ULONG n = RTL_NUMBER_OF(hThreads), m = 0;

		do 
		{
			if (*ph = CreateThread(0, 0, (PTHREAD_START_ROUTINE)TT00, &td, 0, 0))
			{
				ph++, m++;
			}
		} while (--n);

		if (m)
		{
			MessageBoxW(0, 0, L"Start Test", MB_ICONINFORMATION);
			ULONG s = 0;
			RtlRandomEx(&s);

			if (!td.Start(m))
			{
				ULONG time = GetTickCount();

				td.WaitStop();
				DbgPrint("time = %u\n", GetTickCount() - time);
				n = m;
				do 
				{
					--n;
					DbgPrint("%02u: %08u\n", n+1, td._nSC[n]);
				} while (n);

				WaitForMultipleObjects(m, hThreads, TRUE, INFINITE);
			}

			do 
			{
				CloseHandle(*--ph);
			} while (--m);
		}
	}

}
#pragma warning(disable : 4201)

void ep()
{
	CPushLock::InitPushLockApi();
	LockTest();
	ExitProcess(0);
}
