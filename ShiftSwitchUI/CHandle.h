#pragma once
#include <Windows.h>

class CHandle
{
public:
	CHandle(const CHandle& other) = delete;
	CHandle(CHandle&& other) noexcept = default;


	explicit CHandle(HANDLE handle);
	virtual ~CHandle();
	HANDLE get() const;
	operator HANDLE() const;
	bool isValid() const;
	void close();
protected:
	HANDLE m_handle;
};
