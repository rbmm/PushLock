#pragma once

class CPushLock 
{
	struct WaitBlock 
	{
		static ULONG SpinCount;

		enum {
			BIT_SPINNING,
			BIT_EXCLUSIVE,

			FLAG_SPINNING = 1 << BIT_SPINNING,
			FLAG_EXCLUSIVE = 1 << BIT_EXCLUSIVE
		};

		WaitBlock* Next;
		HANDLE ThreadId;
		LONG SC;
		LONG Flags;

		void Wake();
	};

	enum {
		BIT_LOCKED, 
		BIT_EXCLUSIVE, 

		BITS_COUNT,
		SHARE_INC = 1 << BITS_COUNT,
		BITS_MASK = SHARE_INC - 1,

		FLAG_LOCKED = 1 << BIT_LOCKED,
		FLAG_EXCLUSIVE = 1 << BIT_EXCLUSIVE,
	};

	ULONG_PTR Value = 0;

	bool EnterWithWait(ULONG_PTR CurrentValue, LONG Flags);
	void WakeExclusiveWaiter(WaitBlock* last, WaitBlock* prev, ULONG_PTR CurrentValue);
public:

	void AcquireShared();
	void AcquireExclusive();

	void ReleaseExclusive();
	void ReleaseShared();

	bool TryAcquireShared();
	bool TryAcquireExclusive();

	void ConvertExclusiveToShared();
};

VOID WINAPI ReleasePushLockExclusive(PSRWLOCK SRWLock);
VOID WINAPI AcquirePushLockExclusive(PSRWLOCK SRWLock);
VOID WINAPI ReleasePushLockShared(PSRWLOCK SRWLock);
VOID WINAPI AcquirePushLockShared(PSRWLOCK SRWLock);
VOID WINAPI ConvertPushLockExclusiveToShared(PSRWLOCK SRWLock);

BOOL SwitchToPushLock();
BOOL SwitchToSRWLock();

EXTERN_C
WINBASEAPI
BOOLEAN 
WINAPI
RtlConvertSRWLockExclusiveToShared(_Inout_ PSRWLOCK SRWLock);