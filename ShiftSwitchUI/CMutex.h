#pragma once
#include "CHandle.h"

class CMutex : public CHandle
{
public:
	CMutex(
		_In_opt_ LPSECURITY_ATTRIBUTES lpMutexAttributes,
		_In_ BOOL bInitialOwner,
		_In_opt_ LPCWSTR lpName
	);
};
