#pragma once

EXTERN_C_START

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlertThreadByThreadId(
						_In_ HANDLE ThreadId
						);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtWaitForAlertByThreadId(
						 _In_ PVOID Address,
						 _In_opt_ PLARGE_INTEGER Timeout
						 );

NTSYSAPI
PVOID
NTAPI
RtlImageDirectoryEntryToData(
							 _In_ PVOID BaseOfImage,
							 _In_ BOOLEAN MappedAsImage,
							 _In_ USHORT DirectoryEntry,
							 _Out_ PULONG Size
							 );

EXTERN_C_END


