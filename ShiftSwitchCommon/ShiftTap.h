#pragma once

#include <windows.h>

// Tap detection for the Shift keys, independent of the hooks and of any
// Windows state so that it can be unit tested.
//
// A tap is a Shift press without other modifiers and a release while the key
// is still armed. The caller maps the returned action to a keyboard layout.
namespace ShiftTap
{
	enum class Action
	{
		None,
		SwitchLeft,
		SwitchRight,
	};

	struct State
	{
		bool leftShiftDown = false;
		bool rightShiftDown = false;

		void reset()
		{
			leftShiftDown = false;
			rightShiftDown = false;
		}
	};

	// Updates the tap state for one key event.
	//
	// keyCode           virtual-key code of the event.
	// down              true for a key press, false for a release.
	// otherModifierDown true when another modifier (Ctrl, Alt, Win, the other
	//                   Shift) is held, a tap is not recognised then.
	//
	// Returns the layout to activate on release, Action::None otherwise.
	inline Action onKey(const DWORD keyCode, const bool down, const bool otherModifierDown, State& state)
	{
		if (down)
		{
			switch (keyCode)
			{
			case VK_LSHIFT:
				state.rightShiftDown = false;
				if (!otherModifierDown)
					state.leftShiftDown = true;
				break;
			case VK_RSHIFT:
				state.leftShiftDown = false;
				if (!otherModifierDown)
					state.rightShiftDown = true;
				break;
			default:
				state.reset();
				break;
			}
			return Action::None;
		}

		switch (keyCode)
		{
		case VK_LSHIFT:
		{
			const bool armed = state.leftShiftDown;
			state.leftShiftDown = false;
			return armed ? Action::SwitchLeft : Action::None;
		}
		case VK_RSHIFT:
		{
			const bool armed = state.rightShiftDown;
			state.rightShiftDown = false;
			return armed ? Action::SwitchRight : Action::None;
		}
		default:
			return Action::None;
		}
	}
}
