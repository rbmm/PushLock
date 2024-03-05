#include "stdafx.h"

#include "wlog.h"

// /T:[ 1 <= thread count <= 64] 
// /N:[ 1 <= loop count <= 100000] 
// /S:[ 1 <= shared/exclusive - 2^N < 64 ]
// /C:[ 1 <= release/convert - 2^N < 64]

BOOL ParseCmdLine(ULONG* pnThreads, ULONG* pnLoops, ULONG* pnS_E, ULONG* pnR_C)
{
	PULONG p;
	PWSTR pszCmd = GetCommandLineW();
	while (pszCmd = wcschr(pszCmd, '/'))
	{
		switch (*++pszCmd)
		{
		case 'T':
			p = pnThreads;
			break;
		case 'N':
			p = pnLoops;
			break;
		case 'S':
			p = pnS_E;
			break;
		case 'C':
			p = pnR_C;
			break;
		default:
			return FALSE;
		}

		if (':' != *++pszCmd)
		{
			return FALSE;
		}

		ULONG n = wcstoul(pszCmd + 1, &pszCmd, 10);

		switch (*pszCmd)
		{
		case ' ':
		case 0:
			break;
		default:
			return FALSE;
		}

		*p = n;
	}

	return TRUE;
}

BOOL CheckValues(WLog& log, ULONG nThreads, ULONG nLoops, ULONG nS_E, ULONG nR_C)
{
	if (nThreads - 1 > 63)
	{
		log(L"!! Invalid /T:%u must be in range [1,64]\r\n", nThreads);
		return FALSE;
	}

	if (nLoops - 1 > 99999)
	{
		log(L"!! Invalid /N:%u must be in range [1,100000]\r\n", nLoops);
		return FALSE;
	}

	if ((nS_E & (nS_E - 1)) || nS_E - 1 > 63)
	{
		log(L"!! Invalid /S:%u must be in range [1,64] and 2^N\r\n", nS_E);
		return FALSE;
	}

	if ((nR_C & (nR_C - 1)) || nR_C - 1 > 63)
	{
		log(L"!! Invalid /C:%u must be in range [1,64] and 2^N\r\n", nR_C);
		return FALSE;
	}

	return TRUE;
}