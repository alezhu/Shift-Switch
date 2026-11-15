#pragma once
#include <windows.h>
#include <string>
#include "StateCoordinator.h"

typedef BOOL(APIENTRY* SETHOOK)(BOOL bSet);

class HookController : public ILocalHookController
{
public:
	HookController() = default;
	explicit HookController(std::wstring customDllPath);
	~HookController() override;

	HookController(const HookController&) = delete;
	HookController& operator=(const HookController&) = delete;
	HookController(HookController&& other) noexcept;
	HookController& operator=(HookController&& other) noexcept;

	bool Enable() override;
	bool Disable() override;
	bool IsActive() const noexcept override { return m_bActivated; }
	void Shutdown();

	static std::wstring GetDefaultHookDllPath();

private:
	std::wstring m_customDllPath;
	HMODULE m_hLib = nullptr;
	SETHOOK m_pfnSETHOOK = nullptr;
	bool m_bActivated = false;

	friend struct HookControllerTestAccessor;
};
