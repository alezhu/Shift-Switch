#pragma once
#include <Windows.h>

// RAII wrapper for Windows hooks (HHOOK). Automatically unhooks on destruction or reset.
class ScopedHook
{
public:
	ScopedHook() noexcept = default;
	explicit ScopedHook(HHOOK hook) noexcept : m_hook(hook) {}
	~ScopedHook() { reset(); }

	ScopedHook(const ScopedHook&) = delete;
	ScopedHook& operator=(const ScopedHook&) = delete;

	ScopedHook(ScopedHook&& other) noexcept : m_hook(other.m_hook)
	{
		other.m_hook = nullptr;
	}

	ScopedHook& operator=(ScopedHook&& other) noexcept
	{
		if (this != &other)
		{
			reset();
			m_hook = other.m_hook;
			other.m_hook = nullptr;
		}
		return *this;
	}

	void reset(HHOOK newHook = nullptr) noexcept
	{
		if (m_hook != nullptr)
		{
			::UnhookWindowsHookEx(m_hook);
			m_hook = nullptr;
		}
		m_hook = newHook;
	}

	HHOOK release() noexcept
	{
		HHOOK h = m_hook;
		m_hook = nullptr;
		return h;
	}

	HHOOK get() const noexcept { return m_hook; }
	operator HHOOK() const noexcept { return m_hook; }
	explicit operator bool() const noexcept { return m_hook != nullptr; }

private:
	HHOOK m_hook = nullptr;
};

// RAII wrapper for registry keys (HKEY). Automatically closes key on destruction or reset.
class ScopedHKey
{
public:
	ScopedHKey() noexcept = default;
	explicit ScopedHKey(HKEY key) noexcept : m_key(key) {}
	~ScopedHKey() { reset(); }

	ScopedHKey(const ScopedHKey&) = delete;
	ScopedHKey& operator=(const ScopedHKey&) = delete;

	ScopedHKey(ScopedHKey&& other) noexcept : m_key(other.m_key)
	{
		other.m_key = nullptr;
	}

	ScopedHKey& operator=(ScopedHKey&& other) noexcept
	{
		if (this != &other)
		{
			reset();
			m_key = other.m_key;
			other.m_key = nullptr;
		}
		return *this;
	}

	void reset(HKEY newKey = nullptr) noexcept
	{
		if (m_key != nullptr)
		{
			::RegCloseKey(m_key);
			m_key = nullptr;
		}
		m_key = newKey;
	}

	HKEY* addressof() noexcept
	{
		reset();
		return &m_key;
	}

	HKEY release() noexcept
	{
		HKEY k = m_key;
		m_key = nullptr;
		return k;
	}

	HKEY get() const noexcept { return m_key; }
	operator HKEY() const noexcept { return m_key; }
	explicit operator bool() const noexcept { return m_key != nullptr; }

private:
	HKEY m_key = nullptr;
};
