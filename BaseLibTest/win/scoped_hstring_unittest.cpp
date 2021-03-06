// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pch.h"
#include "win/scoped_hstring.h"
#include "strings/utf_string_conversions.h"
#include "win/core_winrt_util.h"
#include "win/windows_version.h"

namespace base::win {

	namespace {

		constexpr wchar_t kTestString1[] = L"123";
		constexpr wchar_t kTestString2[] = L"456789";

	}  // namespace

	TEST(ScopedHStringTest, Init) {
		// ScopedHString requires WinRT core functions, which are not available in
		// older versions.
		if (GetVersion() < Version::WIN8) {
			EXPECT_FALSE(ScopedHString::ResolveCoreWinRTStringDelayload());
			return;
		}

		EXPECT_TRUE(ScopedHString::ResolveCoreWinRTStringDelayload());

		ScopedHString hstring = ScopedHString::Create(kTestString1);
		std::string buffer = hstring.GetAsUTF8();
		EXPECT_EQ(kTestString1, UTF8ToWide(buffer));
		std::wstring_view contents = hstring.Get();
		EXPECT_EQ(kTestString1, contents);

		hstring.reset();
		EXPECT_TRUE(hstring == NULL);
		EXPECT_EQ(NULL, hstring.get());

		ScopedHString hstring2 = ScopedHString::Create(kTestString2);
		hstring.swap(hstring2);
		EXPECT_TRUE(hstring2 == NULL);

		buffer = hstring.GetAsUTF8();
		EXPECT_EQ(kTestString2, UTF8ToWide(buffer));
		contents = hstring.Get();
		EXPECT_EQ(kTestString2, contents);
	}
} // namespace base
 