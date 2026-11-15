#pragma once

#include <cstdio>
#include <string>
#include <vector>

namespace TestHarness
{
	inline int g_checks = 0;
	inline int g_failures = 0;

	inline void check(const bool condition, const char* expression, const char* test, const int line)
	{
		++g_checks;
		if (!condition)
		{
			++g_failures;
			std::printf("  FAIL %s (line %d): %s\n", test, line, expression);
		}
	}
}

#define CHECK(expr) ::TestHarness::check((expr), #expr, __func__, __LINE__)
#define CHECK_TRUE(expr) CHECK((expr))
#define CHECK_FALSE(expr) CHECK(!(expr))
#define CHECK_EQUAL(a, b) CHECK((a) == (b))
