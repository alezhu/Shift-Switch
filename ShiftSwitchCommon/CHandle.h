#pragma once
#include <Windows.h>

class CHandle
{
public:
	CHandle(const CHandle& other) = delete;
	CHandle& operator=(const CHandle& other) = delete;
	CHandle(CHandle&& other) noexcept;
	CHandle& operator=(CHandle&& other) noexcept;

	CHandle();
	explicit CHandle(HANDLE handle);
	virtual ~CHandle();
	HANDLE get() const;
	operator HANDLE() const;
	bool isValid() const;
	void close();
	void reset(HANDLE handle = INVALID_HANDLE_VALUE);
	HANDLE release();
protected:
	HANDLE m_handle;
};
