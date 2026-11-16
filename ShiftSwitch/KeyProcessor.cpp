#include "pch.h"
#include "KeyProcessor.h"
#include "HookManager.h"
#include "AsyncLogger.h"

void ProcessKeyEvent(ShiftTap::State& state, const DWORD keyCode, const bool down, WORD /*repeatCount*/, HOOK_CONTEXT& ctx)
{
	const bool shiftKey = keyCode == VK_LSHIFT || keyCode == VK_RSHIFT;
	const bool otherModifierDown = down && shiftKey
		&& checkIsSomeModifKeyDownExcept(static_cast<BYTE>(keyCode));

	const auto action = ShiftTap::onKey(keyCode, down, otherModifierDown, state);
	if (shiftKey || action != ShiftTap::Action::None)
	{
		LOG_DEBUG(L"ProcessKeyEvent: keyCode=0x%x, down=%d, otherMod=%d, action=%d",
			keyCode, (int)down, (int)otherModifierDown, (int)action);
	}

	switch (action)
	{
	case ShiftTap::Action::SwitchLeft:
	{
		const auto hCurKL = GetKeyboardLayout(ctx.inputThreadId);
		LOG_DEBUG(L"SwitchLeft: inputThreadId=%u, curKL=%p (low=0x%x), engKL=%p (low=0x%x)",
			ctx.inputThreadId, hCurKL, LOWORD(reinterpret_cast<UINT_PTR>(hCurKL)),
			hklEnglish, LOWORD(reinterpret_cast<UINT_PTR>(hklEnglish)));
		if (LOWORD(reinterpret_cast<UINT_PTR>(hCurKL)) != LOWORD(reinterpret_cast<UINT_PTR>(hklEnglish))) {
			LogSwitch(L"Switch to English");
			ActivateLayout(hklEnglish, ctx);
		}
		break;
	}
	case ShiftTap::Action::SwitchRight:
	{
		const auto hCurKL = GetKeyboardLayout(ctx.inputThreadId);
		LOG_DEBUG(L"SwitchRight: inputThreadId=%u, curKL=%p (low=0x%x), rusKL=%p (low=0x%x)",
			ctx.inputThreadId, hCurKL, LOWORD(reinterpret_cast<UINT_PTR>(hCurKL)),
			hklRussian, LOWORD(reinterpret_cast<UINT_PTR>(hklRussian)));
		if (LOWORD(reinterpret_cast<UINT_PTR>(hCurKL)) != LOWORD(reinterpret_cast<UINT_PTR>(hklRussian))) {
			LogSwitch(L"Switch to Russian");
			ActivateLayout(hklRussian, ctx);
		}
		break;
	}
	case ShiftTap::Action::None:
		break;
	}
}
