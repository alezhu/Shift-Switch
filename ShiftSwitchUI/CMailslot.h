#pragma once
#include <string_view>

#include "CHandle.h"

class CMailslot : public CHandle
{
public:
	explicit CMailslot(bool bForRead);
	void send(const std::wstring_view& message);
};
