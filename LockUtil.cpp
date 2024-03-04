#include "stdafx.h"

#include "pushlock.h"
#include "lockutil.h"

VOID WINAPI ReleasePushLockExclusive(PSRWLOCK SRWLock)
{
	reinterpret_cast<CPushLock*>(SRWLock)->ReleaseExclusive();
}

VOID WINAPI ReleasePushLockShared(PSRWLOCK SRWLock)
{
	reinterpret_cast<CPushLock*>(SRWLock)->ReleaseShared();
}

VOID WINAPI AcquirePushLockExclusive(PSRWLOCK SRWLock)
{
	reinterpret_cast<CPushLock*>(SRWLock)->AcquireExclusive();
}

VOID WINAPI AcquirePushLockShared(PSRWLOCK SRWLock)
{
	reinterpret_cast<CPushLock*>(SRWLock)->AcquireShared();
}

VOID WINAPI ConvertPushLockExclusiveToShared(PSRWLOCK SRWLock)
{
	reinterpret_cast<CPushLock*>(SRWLock)->ConvertExclusiveToShared();
}

EXTERN_C extern PVOID __imp_ReleaseSRWLockExclusive;
EXTERN_C extern PVOID __imp_ReleaseSRWLockShared;
EXTERN_C extern PVOID __imp_AcquireSRWLockExclusive;
EXTERN_C extern PVOID __imp_AcquireSRWLockShared;
EXTERN_C extern PVOID __imp_RtlConvertSRWLockExclusiveToShared;

EXTERN_C extern IMAGE_DOS_HEADER __ImageBase;

BOOL SwitchToPushLock()
{
	ULONG s;
	if (PVOID pv = RtlImageDirectoryEntryToData(&__ImageBase, TRUE, IMAGE_DIRECTORY_ENTRY_IAT, &s))
	{
		ULONG op;
		if (VirtualProtect(pv, s, PAGE_READWRITE, &op))
		{
			__imp_ReleaseSRWLockExclusive = ReleasePushLockExclusive;
			__imp_ReleaseSRWLockShared = ReleasePushLockShared;
			__imp_AcquireSRWLockExclusive = AcquirePushLockExclusive;
			__imp_AcquireSRWLockShared = AcquirePushLockShared;
			__imp_RtlConvertSRWLockExclusiveToShared = ConvertPushLockExclusiveToShared;

			VirtualProtect(pv, s, op, &op);

			return TRUE;
		}
	}

	return FALSE;
}

BOOL SwitchToSRWLock()
{
	if (HMODULE hmod = GetModuleHandleW(L"NTDLL"))
	{
		ULONG s;
		if (PVOID pv = RtlImageDirectoryEntryToData(&__ImageBase, TRUE, IMAGE_DIRECTORY_ENTRY_IAT, &s))
		{
			ULONG op;
			if (VirtualProtect(pv, s, PAGE_READWRITE, &op))
			{
				__imp_ReleaseSRWLockExclusive = GetProcAddress(hmod, "RtlReleaseSRWLockExclusive");
				__imp_ReleaseSRWLockShared = GetProcAddress(hmod, "RtlReleaseSRWLockShared");
				__imp_AcquireSRWLockExclusive = GetProcAddress(hmod, "RtlAcquireSRWLockExclusive");
				__imp_AcquireSRWLockShared = GetProcAddress(hmod, "RtlAcquireSRWLockShared");
				__imp_RtlConvertSRWLockExclusiveToShared = GetProcAddress(hmod, "RtlConvertSRWLockExclusiveToShared");

				VirtualProtect(pv, s, op, &op);

				return TRUE;
			}
		}
	}

	return FALSE;
}