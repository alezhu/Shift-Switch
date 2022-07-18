#include "pch.h"
#include "CHandle.h"

CHandle::CHandle(HANDLE handle)
	:m_handle(handle)
{}

CHandle::~CHandle()
{
	close();
}


HANDLE CHandle::get() const
{
	return m_handle;
}

CHandle::operator HANDLE () const
{
	return m_handle;
}

bool CHandle::isValid() const
{
	return get() != INVALID_HANDLE_VALUE;
}

void CHandle::close()
{
	if (isValid()) {
		::CloseHandle(m_handle);
		m_handle = INVALID_HANDLE_VALUE;
	}
}
