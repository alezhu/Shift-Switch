#include "pch.h"
#include "CMutex.h"

CMutex::CMutex(
	_In_opt_ LPSECURITY_ATTRIBUTES lpMutexAttributes,
	_In_ BOOL bInitialOwner,
	_In_opt_ LPCWSTR lpName
) :
	CHandle(::CreateMutexW(lpMutexAttributes, bInitialOwner, lpName))
{
}
