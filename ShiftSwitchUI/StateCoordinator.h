#pragma once

class ILocalHookController
{
public:
	virtual ~ILocalHookController() = default;
	virtual bool Enable() = 0;
	virtual bool Disable() = 0;
	virtual bool IsActive() const noexcept = 0;
};

class IChildProcessHook
{
public:
	virtual ~IChildProcessHook() = default;
	virtual bool EnableChild() = 0;
	virtual bool DisableChild() = 0;
	virtual bool QueryChildState(bool& isActive) = 0;
	virtual bool HasChild() const = 0;
};

class StateCoordinator
{
public:
	StateCoordinator(ILocalHookController& localHook, IChildProcessHook& childHook) noexcept
		: m_localHook(localHook), m_childHook(childHook)
	{
	}

	bool IsEnabled() const noexcept
	{
		return m_localHook.IsActive();
	}

	bool SetEnabled(bool enable, bool force = false)
	{
		if (!force && enable == IsEnabled())
		{
			return true;
		}

		if (!enable) // Disable
		{
			// 1. Disable hook in main process
			if (!m_localHook.Disable())
			{
				return false;
			}

			// 2. Disable hook in child process if present
			if (m_childHook.HasChild())
			{
				if (!m_childHook.DisableChild())
				{
					// 3. Rollback main process if child did not acknowledge or failed
					m_localHook.Enable();
					return false;
				}
			}

			// 4. Hook successfully disabled across processes
			return true;
		}
		else // Enable
		{
			// 1. Enable hook in main process
			if (!m_localHook.Enable())
			{
				return false;
			}

			// 2. Enable hook in child process if present
			if (m_childHook.HasChild())
			{
				if (!m_childHook.EnableChild())
				{
					// 3. Rollback main process if child did not acknowledge or failed
					m_localHook.Disable();
					return false;
				}
			}

			// 4. Hook successfully enabled across processes
			return true;
		}
	}

	bool OnChildRegistered()
	{
		if (!m_childHook.HasChild())
		{
			if (IsEnabled())
			{
				m_localHook.Disable();
			}
			return false;
		}

		if (IsEnabled())
		{
			if (!m_childHook.EnableChild())
			{
				// Rollback main process if child did not acknowledge or failed
				m_localHook.Disable();
				return false;
			}
		}
		else
		{
			if (!m_childHook.DisableChild())
			{
				return false;
			}
		}

		return true;
	}

private:
	ILocalHookController& m_localHook;
	IChildProcessHook& m_childHook;
};
