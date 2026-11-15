#include "pch.h"
#include "CHandle.h"

CHandle::CHandle()
	: m_handle(INVALID_HANDLE_VALUE)
{}

CHandle::CHandle(HANDLE handle)
	: m_handle(handle)
{}

CHandle::~CHandle()
{
	close();
}

CHandle::CHandle(CHandle&& other) noexcept
	: m_handle(other.m_handle)
{
	other.m_handle = INVALID_HANDLE_VALUE;
}

CHandle& CHandle::operator=(CHandle&& other) noexcept
{
	if (this != &other)
	{
		close();
		m_handle = other.m_handle;
		other.m_handle = INVALID_HANDLE_VALUE;
	}
	return *this;
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
	return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE;
}

void CHandle::close()
{
	if (isValid()) {
		::CloseHandle(m_handle);
		m_handle = INVALID_HANDLE_VALUE;
	}
}

void CHandle::reset(HANDLE handle)
{
	if (m_handle != handle)
	{
		close();
		m_handle = handle;
	}
}

HANDLE CHandle::release()
{
	HANDLE h = m_handle;
	m_handle = INVALID_HANDLE_VALUE;
	return h;
}
