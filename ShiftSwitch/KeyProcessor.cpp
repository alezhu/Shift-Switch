#include "pch.h"
#include "KeyProcessor.h"
#include "HookManager.h"
#include "AsyncLogger.h"

void ProcessKeyEvent(ShiftTap::State& state, const DWORD keyCode, const bool down, WORD /*repeatCount*/, HOOK_CONTEXT& ctx)
{
	const bool shiftKey = keyCode == VK_LSHIFT || keyCode == VK_RSHIFT;
	const bool otherModifierDown = down && shiftKey
		&& checkIsSomeModifKeyDownExcept(static_cast<BYTE>(keyCode));

	switch (ShiftTap::onKey(keyCode, down, otherModifierDown, state))
	{
	case ShiftTap::Action::SwitchLeft:
	{
		const auto hCurKL = GetKeyboardLayout(ctx.inputThreadId);
		if (hCurKL != hklEnglish) {
			LogSwitch(L"Switch to English");
			ActivateLayout(hklEnglish, ctx);
		}
		break;
	}
	case ShiftTap::Action::SwitchRight:
	{
		const auto hCurKL = GetKeyboardLayout(ctx.inputThreadId);
		if (hCurKL != hklRussian) {
			LogSwitch(L"Switch to Russian");
			ActivateLayout(hklRussian, ctx);
		}
		break;
	}
	case ShiftTap::Action::None:
		break;
	}
}
