#pragma once

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Low-level UTF handling functions. Most code will want to use the functions
// in utf_string_conversions.h

#include <cstddef>
#include <cstdint>

#include "base_export.h"
#include <string>

namespace base
{

	inline bool IsValidCodepoint(uint32_t code_point)
	{
		// Excludes the surrogate code points ([0xD800, 0xDFFF]) and
		// codepoints larger than 0x10FFFF (the highest codepoint allowed).
		// Non-characters and unassigned codepoints are allowed.
		return code_point < 0xD800u ||
			(code_point >= 0xE000u && code_point <= 0x10FFFFu);
	}

	inline bool IsValidCharacter(uint32_t code_point)
	{
		// Excludes non-characters (U+FDD0..U+FDEF, and all codepoints ending in
		// 0xFFFE or 0xFFFF) from the set of valid code points.
		return code_point < 0xD800u || (code_point >= 0xE000u &&
			code_point < 0xFDD0u) || (code_point > 0xFDEFu &&
				code_point <= 0x10FFFFu && (code_point & 0xFFFEu) != 0xFFFEu);
	}

	// ReadUnicodeCharacter --------------------------------------------------------

	// Reads a UTF-8 stream, placing the next code point into the given output
	// |*code_point|. |src| represents the entire string to read, and |*char_index|
	// is the character offset within the string to start reading at. |*char_index|
	// will be updated to index the last character read, such that incrementing it
	// (as in a for loop) will take the reader to the next character.
	//
	// Returns true on success. On false, |*code_point| will be invalid.
	BASE_EXPORT bool ReadUnicodeCharacter(const char* src, int32_t src_len, int32_t* char_index, uint32_t* code_point_out);

	// Reads a UTF-16 character. The usage is the same as the 8-bit version above.
	BASE_EXPORT bool ReadUnicodeCharacter(const wchar_t* src, int32_t src_len, int32_t* char_index, uint32_t* code_point);

	// WriteUnicodeCharacter -------------------------------------------------------

	// Appends a UTF-8 character to the given 8-bit string.  Returns the number of
	// bytes written.
	BASE_EXPORT size_t WriteUnicodeCharacter(uint32_t code_point, std::string* output);

	// Appends the given code point as a UTF-16 character to the given 16-bit
	// string.  Returns the number of 16-bit values written.
	BASE_EXPORT size_t WriteUnicodeCharacter(uint32_t code_point, std::wstring* output);

	// Generalized Unicode converter -----------------------------------------------

	// Guesses the length of the output in UTF-8 in bytes, clears that output
	// string, and reserves that amount of space.  We assume that the input
	// character types are unsigned, which will be true for UTF-16 and -32 on our
	// systems.
	void PrepareForUTF8Output(const wchar_t* src, size_t src_len, std::string* output);

	// Prepares an output buffer (containing either UTF-16 or -32 data) given some
	// UTF-8 input that will be converted to it.  See PrepareForUTF8Output().
	void PrepareForUTF16Or32Output(const char* src, size_t src_len, std::wstring* output);

}  // namespace base