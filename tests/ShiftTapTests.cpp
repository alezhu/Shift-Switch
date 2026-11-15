// Unit tests for the Shift tap state machine in ShiftSwitch\ShiftTap.h.
//
// The tests use a minimal dependency-free harness so that they build and run
// on any machine with the C++ toolset, without NuGet or vcpkg packages.

#include "TestHarness.h"
#include "ShiftTapTests.h"
#include "ShiftTap.h"
#include "ScopeExit.h"
#include "ScopedResource.h"
#include "StateCoordinator.h"

#include <cstdio>

// Left Shift tap: press arms the left layout, release switches to it.
static void leftShiftTapSwitchesLeft()
{
	ShiftTap::State state;
	CHECK(ShiftTap::onKey(VK_LSHIFT, true, false, state) == ShiftTap::Action::None);
	CHECK(state.leftShiftDown);
	CHECK(!state.rightShiftDown);
	CHECK(ShiftTap::onKey(VK_LSHIFT, false, false, state) == ShiftTap::Action::SwitchLeft);
	CHECK(!state.leftShiftDown);
}

// Right Shift tap: press arms the right layout, release switches to it.
static void rightShiftTapSwitchesRight()
{
	ShiftTap::State state;
	CHECK(ShiftTap::onKey(VK_RSHIFT, true, false, state) == ShiftTap::Action::None);
	CHECK(state.rightShiftDown);
	CHECK(!state.leftShiftDown);
	CHECK(ShiftTap::onKey(VK_RSHIFT, false, false, state) == ShiftTap::Action::SwitchRight);
	CHECK(!state.rightShiftDown);
}

// A Shift press with another modifier held is a shortcut, not a tap.
static void shiftPressWithOtherModifierIsNotATap()
{
	ShiftTap::State state;
	CHECK(ShiftTap::onKey(VK_LSHIFT, true, true, state) == ShiftTap::Action::None);
	CHECK(!state.leftShiftDown);
	CHECK(ShiftTap::onKey(VK_LSHIFT, false, false, state) == ShiftTap::Action::None);
}

// Any other key press cancels an armed tap (Shift+letter, Shift+digit, ...).
static void otherKeyPressResetsArmedShift()
{
	ShiftTap::State state;
	ShiftTap::onKey(VK_LSHIFT, true, false, state);
	CHECK(ShiftTap::onKey('A', true, false, state) == ShiftTap::Action::None);
	CHECK(!state.leftShiftDown);
	CHECK(ShiftTap::onKey(VK_LSHIFT, false, false, state) == ShiftTap::Action::None);
}

// Pressing the other Shift replaces the armed state instead of adding to it.
static void otherShiftPressReplacesArmedState()
{
	ShiftTap::State state;
	ShiftTap::onKey(VK_LSHIFT, true, false, state);
	ShiftTap::onKey(VK_RSHIFT, true, false, state);
	CHECK(!state.leftShiftDown);
	CHECK(state.rightShiftDown);
	CHECK(ShiftTap::onKey(VK_LSHIFT, false, false, state) == ShiftTap::Action::None);
	CHECK(ShiftTap::onKey(VK_RSHIFT, false, false, state) == ShiftTap::Action::SwitchRight);
}

// Releasing a Shift that was not armed does nothing.
static void releaseWithoutPressDoesNothing()
{
	ShiftTap::State state;
	CHECK(ShiftTap::onKey(VK_LSHIFT, false, false, state) == ShiftTap::Action::None);
	CHECK(ShiftTap::onKey(VK_RSHIFT, false, false, state) == ShiftTap::Action::None);
}

// Auto-repeat keeps the tap armed.
static void repeatedPressKeepsTapArmed()
{
	ShiftTap::State state;
	ShiftTap::onKey(VK_LSHIFT, true, false, state);
	CHECK(ShiftTap::onKey(VK_LSHIFT, true, false, state) == ShiftTap::Action::None);
	CHECK(state.leftShiftDown);
	CHECK(ShiftTap::onKey(VK_LSHIFT, false, false, state) == ShiftTap::Action::SwitchLeft);
}

// Releasing a non-Shift key does not touch the armed state.
static void nonShiftReleaseKeepsTapArmed()
{
	ShiftTap::State state;
	ShiftTap::onKey(VK_LSHIFT, true, false, state);
	CHECK(ShiftTap::onKey('A', false, false, state) == ShiftTap::Action::None);
	CHECK(state.leftShiftDown);
}

// reset() drops a pending tap, for example when the foreground window changes.
static void resetDropsPendingTap()
{
	ShiftTap::State state;
	ShiftTap::onKey(VK_RSHIFT, true, false, state);
	state.reset();
	CHECK(!state.leftShiftDown);
	CHECK(!state.rightShiftDown);
	CHECK(ShiftTap::onKey(VK_RSHIFT, false, false, state) == ShiftTap::Action::None);
}

static void scopeExitRunsOnScopeEnd()
{
	bool executed = false;
	{
		ScopeExit guard([&] { executed = true; });
		CHECK(!executed);
	}
	CHECK(executed);
}

static void scopeExitDismissPreventsExecution()
{
	bool executed = false;
	{
		ScopeExit guard([&] { executed = true; });
		guard.dismiss();
	}
	CHECK(!executed);
}

static void scopeExitMoveTransfersOwnership()
{
	int count = 0;
	{
		ScopeExit guard1([&] { ++count; });
		ScopeExit guard2(std::move(guard1));
		CHECK(count == 0);
	}
	CHECK(count == 1);
}

static void scopedHookMoveAndRelease()
{
	ScopedHook hook;
	CHECK(!hook);
	CHECK(hook.get() == nullptr);

	HHOOK dummy = reinterpret_cast<HHOOK>(static_cast<uintptr_t>(0x1234));
	ScopedHook hook1(dummy);
	CHECK(hook1);
	CHECK(hook1.get() == dummy);

	ScopedHook hook2(std::move(hook1));
#pragma warning(push)
#pragma warning(disable: 26800) // Unit test explicitly verifies moved-from object state
	CHECK(!hook1);
	CHECK(hook1.get() == nullptr);
#pragma warning(pop)
	CHECK(hook2);
	CHECK(hook2.get() == dummy);

	HHOOK released = hook2.release();
	CHECK(released == dummy);
	CHECK(!hook2);
}

static void scopedHKeyMoveAndRelease()
{
	ScopedHKey key;
	CHECK(!key);
	CHECK(key.get() == nullptr);

	HKEY dummy = reinterpret_cast<HKEY>(static_cast<uintptr_t>(0x5678));
	ScopedHKey key1(dummy);
	CHECK(key1);
	CHECK(key1.get() == dummy);

	ScopedHKey key2(std::move(key1));
#pragma warning(push)
#pragma warning(disable: 26800) // Unit test explicitly verifies moved-from object state
	CHECK(!key1);
	CHECK(key1.get() == nullptr);
#pragma warning(pop)
	CHECK(key2);
	CHECK(key2.get() == dummy);

	HKEY released = key2.release();
	CHECK(released == dummy);
	CHECK(!key2);
}

class MockLocalHook : public ILocalHookController
{
public:
	bool active = false;
	bool failEnable = false;
	bool failDisable = false;
	int enableCount = 0;
	int disableCount = 0;

	bool Enable() override
	{
		++enableCount;
		if (failEnable) return false;
		active = true;
		return true;
	}

	bool Disable() override
	{
		++disableCount;
		if (failDisable) return false;
		active = false;
		return true;
	}

	bool IsActive() const noexcept override
	{
		return active;
	}
};

class MockChildHook : public IChildProcessHook
{
public:
	bool hasChild = true;
	bool active = false;
	bool failEnable = false;
	bool failDisable = false;
	int enableCount = 0;
	int disableCount = 0;

	bool HasChild() const override
	{
		return hasChild;
	}

	bool EnableChild() override
	{
		++enableCount;
		if (failEnable) return false;
		active = true;
		return true;
	}

	bool DisableChild() override
	{
		++disableCount;
		if (failDisable) return false;
		active = false;
		return true;
	}

	bool QueryChildState(bool& isActive) override
	{
		isActive = active;
		return true;
	}
};

static void stateCoordinatorEnableBothSucceed()
{
	MockLocalHook local;
	MockChildHook child;
	StateCoordinator coord(local, child);

	CHECK(!coord.IsEnabled());
	CHECK(coord.SetEnabled(true));
	CHECK(coord.IsEnabled());
	CHECK(local.active);
	CHECK(child.active);
	CHECK(local.enableCount == 1);
	CHECK(child.enableCount == 1);

	// Idempotency: re-enabling should be a no-op
	CHECK(coord.SetEnabled(true));
	CHECK(local.enableCount == 1);
	CHECK(child.enableCount == 1);
}

static void stateCoordinatorDisableBothSucceed()
{
	MockLocalHook local;
	local.active = true;
	MockChildHook child;
	child.active = true;
	StateCoordinator coord(local, child);

	CHECK(coord.IsEnabled());
	CHECK(coord.SetEnabled(false));
	CHECK(!coord.IsEnabled());
	CHECK(!local.active);
	CHECK(!child.active);
	CHECK(local.disableCount == 1);
	CHECK(child.disableCount == 1);

	// Idempotency: re-disabling should be a no-op
	CHECK(coord.SetEnabled(false));
	CHECK(local.disableCount == 1);
	CHECK(child.disableCount == 1);
}

static void stateCoordinatorEnableRollbackOnChildFailure()
{
	MockLocalHook local;
	MockChildHook child;
	child.failEnable = true;
	StateCoordinator coord(local, child);

	CHECK(!coord.IsEnabled());
	// Enable fails on child -> rollback local to disabled
	CHECK(!coord.SetEnabled(true));
	CHECK(!coord.IsEnabled());
	CHECK(!local.active);
	CHECK(local.enableCount == 1);
	CHECK(local.disableCount == 1); // Rollback performed
	CHECK(child.enableCount == 1);
}

static void stateCoordinatorDisableRollbackOnChildFailure()
{
	MockLocalHook local;
	local.active = true;
	MockChildHook child;
	child.active = true;
	child.failDisable = true;
	StateCoordinator coord(local, child);

	CHECK(coord.IsEnabled());
	// Disable fails on child -> rollback local to enabled
	CHECK(!coord.SetEnabled(false));
	CHECK(coord.IsEnabled());
	CHECK(local.active);
	CHECK(local.disableCount == 1);
	CHECK(local.enableCount == 1); // Rollback performed
	CHECK(child.disableCount == 1);
}

static void stateCoordinatorSingleProcessWithoutChild()
{
	MockLocalHook local;
	MockChildHook child;
	child.hasChild = false;
	StateCoordinator coord(local, child);

	CHECK(coord.SetEnabled(true));
	CHECK(coord.IsEnabled());
	CHECK(local.active);
	CHECK(child.enableCount == 0);

	CHECK(coord.SetEnabled(false));
	CHECK(!coord.IsEnabled());
	CHECK(!local.active);
	CHECK(child.disableCount == 0);
}

static void stateCoordinatorOnChildRegisteredSyncsState()
{
	MockLocalHook local;
	MockChildHook child;
	StateCoordinator coord(local, child);

	// Case 1: Main is enabled when child registers and child succeeds
	local.active = true;
	child.active = false;
	CHECK(coord.OnChildRegistered());
	CHECK(child.active);
	CHECK(child.enableCount == 1);
	CHECK(coord.IsEnabled());

	// Case 2: Main is disabled when child registers and child succeeds
	local.active = false;
	child.active = true;
	CHECK(coord.OnChildRegistered());
	CHECK(!child.active);
	CHECK(child.disableCount == 1);
	CHECK(!coord.IsEnabled());
}

static void stateCoordinatorOnChildRegisteredRollbackOnChildFailure()
{
	MockLocalHook local;
	local.active = true;
	MockChildHook child;
	child.active = false;
	child.failEnable = true;
	StateCoordinator coord(local, child);

	CHECK(coord.IsEnabled());
	// Child enable fails -> rollback local to disabled
	CHECK(!coord.OnChildRegistered());
	CHECK(!coord.IsEnabled());
	CHECK(!local.active);
	CHECK(local.disableCount == 1); // Rollback performed
	CHECK(child.enableCount == 1);
}

static void stateCoordinatorOnChildRegisteredFailsWhenDisableChildFails()
{
	MockLocalHook local;
	local.active = false;
	MockChildHook child;
	child.active = true;
	child.failDisable = true;
	StateCoordinator coord(local, child);

	CHECK(!coord.IsEnabled());
	CHECK(!coord.OnChildRegistered());
	CHECK(!coord.IsEnabled());
	CHECK(!local.active);
	CHECK(child.disableCount == 1);
}

static void stateCoordinatorOnChildRegisteredWithoutChild()
{
	MockLocalHook local;
	local.active = true;
	MockChildHook child;
	child.hasChild = false;
	StateCoordinator coord(local, child);

	CHECK(coord.IsEnabled());
	CHECK(!coord.OnChildRegistered());
	CHECK(!coord.IsEnabled());
	CHECK(!local.active);
	CHECK(local.disableCount == 1);
	CHECK(child.enableCount == 0);
}

static void stateCoordinatorFailsWhenLocalFails()
{
	MockLocalHook local;
	local.failEnable = true;
	MockChildHook child;
	StateCoordinator coord(local, child);

	CHECK(!coord.SetEnabled(true));
	CHECK(!coord.IsEnabled());
	CHECK(child.enableCount == 0); // Child never called

	local.failEnable = false;
	local.active = true;
	local.failDisable = true;
	CHECK(!coord.SetEnabled(false));
	CHECK(coord.IsEnabled());
	CHECK(child.disableCount == 0); // Child never called
}

void runShiftTapTests()
{
	std::printf("--- ShiftTap & State Machine Unit Tests ---\n");
	leftShiftTapSwitchesLeft();
	rightShiftTapSwitchesRight();
	shiftPressWithOtherModifierIsNotATap();
	otherKeyPressResetsArmedShift();
	otherShiftPressReplacesArmedState();
	releaseWithoutPressDoesNothing();
	repeatedPressKeepsTapArmed();
	nonShiftReleaseKeepsTapArmed();
	resetDropsPendingTap();

	scopeExitRunsOnScopeEnd();
	scopeExitDismissPreventsExecution();
	scopeExitMoveTransfersOwnership();
	scopedHookMoveAndRelease();
	scopedHKeyMoveAndRelease();

	stateCoordinatorEnableBothSucceed();
	stateCoordinatorDisableBothSucceed();
	stateCoordinatorEnableRollbackOnChildFailure();
	stateCoordinatorDisableRollbackOnChildFailure();
	stateCoordinatorSingleProcessWithoutChild();
	stateCoordinatorOnChildRegisteredSyncsState();
	stateCoordinatorOnChildRegisteredRollbackOnChildFailure();
	stateCoordinatorOnChildRegisteredFailsWhenDisableChildFails();
	stateCoordinatorOnChildRegisteredWithoutChild();
	stateCoordinatorFailsWhenLocalFails();
}
