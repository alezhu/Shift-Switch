#pragma once
#include <utility>

template <typename F>
class ScopeExit
{
public:
	explicit ScopeExit(F&& fn) : m_fn(std::move(fn)), m_active(true) {}
	explicit ScopeExit(const F& fn) : m_fn(fn), m_active(true) {}

	~ScopeExit()
	{
		if (m_active)
			m_fn();
	}

	void dismiss() noexcept
	{
		m_active = false;
	}

	ScopeExit(const ScopeExit&) = delete;
	ScopeExit& operator=(const ScopeExit&) = delete;

	ScopeExit(ScopeExit&& other) noexcept
		: m_fn(std::move(other.m_fn)), m_active(other.m_active)
	{
		other.m_active = false;
	}

	ScopeExit& operator=(ScopeExit&&) = delete;

private:
	F m_fn;
	bool m_active;
};

template <typename F>
ScopeExit(F) -> ScopeExit<F>;

template <typename F>
ScopeExit<std::decay_t<F>> MakeScopeExit(F&& fn)
{
	return ScopeExit<std::decay_t<F>>(std::forward<F>(fn));
}
