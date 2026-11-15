#pragma once
#include <windows.h>
#include "ShiftTap.h"
#include "LayoutSwitcher.h"

void ProcessKeyEvent(ShiftTap::State& state, DWORD keyCode, bool down, WORD repeatCount, HOOK_CONTEXT& ctx);
