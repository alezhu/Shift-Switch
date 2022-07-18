#include "pch.h"
#include "CMailslot.h"

constexpr auto mailSlotName = LR"(\\.\mailslot\ShiftSwitch)";

CMailslot::CMailslot(bool bForRead)
	:CHandle(bForRead
		? CreateMailslot(
			mailSlotName,
			0,
			MAILSLOT_WAIT_FOREVER,
			nullptr
		)
		: ::CreateFile(
			mailSlotName,
			GENERIC_WRITE,
			FILE_SHARE_READ,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			nullptr
		))
{
}

void CMailslot::send(const std::wstring_view& message)
{
	if (!isValid() || message.length() == 0) return;
	const DWORD size = (message.length() + 1) * sizeof(message[0]);
	DWORD dwWritten;
	auto result = WriteFile(
		get(),
		message.data(),
		size,
		&dwWritten,
		nullptr
	);
}
