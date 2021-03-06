// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pch.h"
#include "win/hstring_reference.h"
#include "win/scoped_hstring.h"

#include "win/windows_version.h"

namespace base::win
{

		namespace {

			constexpr wchar_t kTestString[] = L"123";
			constexpr wchar_t kEmptyString[] = L"";

			void VerifyHSTRINGEquals(HSTRING hstring, const wchar_t* test_string) {
				const ScopedHString scoped_hstring(hstring);
				const std::wstring_view hstring_contents = scoped_hstring.Get();
				EXPECT_EQ(hstring_contents.compare(test_string), 0);
			}

		}  // namespace

		TEST(HStringReferenceTest, Init) {
			// ScopedHString requires WinRT core functions, which are not available in
			// older versions.
			if (GetVersion() < Version::WIN8) {
				EXPECT_FALSE(HStringReference::ResolveCoreWinRTStringDelayload());
				return;
			}

			EXPECT_TRUE(HStringReference::ResolveCoreWinRTStringDelayload());
			EXPECT_TRUE(ScopedHString::ResolveCoreWinRTStringDelayload());

			const HStringReference string(kTestString);
			EXPECT_NE(string.Get(), nullptr);
			VerifyHSTRINGEquals(string.Get(), kTestString);

			// Empty strings come back as null HSTRINGs, a valid HSTRING.
			const HStringReference empty_string(kEmptyString);
			EXPECT_EQ(empty_string.Get(), nullptr);
			VerifyHSTRINGEquals(empty_string.Get(), kEmptyString);

			// Passing a zero length and null string should also return a null HSTRING.
			const HStringReference null_string(nullptr, 0);
			EXPECT_EQ(null_string.Get(), nullptr);
			VerifyHSTRINGEquals(null_string.Get(), kEmptyString);
		}
} // namespace base
