// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <cstddef>
#include <cstdint>

#include <string>
#include <vector>

#include "base_export.h"
#include "containers/span.h"

// ----------------------------------------------------------------------------
// IMPORTANT MESSAGE FROM YOUR SPONSOR
//
// This file contains no "wstring" variants. New code should use std::wstring. If
// you need to make old code work, use the UTF8 version and convert. Please do
// not add wstring variants.
//
// Please do not add "convenience" functions for converting strings to integers
// that return the value and ignore success/failure. That encourages people to
// write code that doesn't properly handle the error conditions.
//
// DO NOT use these functions in any UI unless it's NOT localized on purpose.
// Instead, use base::MessageFormatter for a complex message with numbers
// (integer, float, double) embedded or base::Format{Number,Double,Percent} to
// just format a single number/percent. Note that some languages use native
// digits instead of ASCII digits while others use a group separator or decimal
// point different from ',' and '.'. Using these functions in the UI would lead
// numbers to be formatted in a non-native way.
// ----------------------------------------------------------------------------

namespace base {

	// Number -> string conversions ------------------------------------------------

	// Ignores locale! see warning above.
	BASE_EXPORT std::string NumberToString(int value);
	BASE_EXPORT std::wstring NumberToString16(int value);
	BASE_EXPORT std::string NumberToString(unsigned int value);
	BASE_EXPORT std::wstring NumberToString16(unsigned int value);
	BASE_EXPORT std::string NumberToString(long value);
	BASE_EXPORT std::wstring NumberToString16(long value);
	BASE_EXPORT std::string NumberToString(unsigned long value);
	BASE_EXPORT std::wstring NumberToString16(unsigned long value);
	BASE_EXPORT std::string NumberToString(long long value);
	BASE_EXPORT std::wstring NumberToString16(long long value);
	BASE_EXPORT std::string NumberToString(unsigned long long value);
	BASE_EXPORT std::wstring NumberToString16(unsigned long long value);
	BASE_EXPORT std::string NumberToString(double value);
	BASE_EXPORT std::wstring NumberToString16(double value);

	// String -> number conversions ------------------------------------------------

	// Perform a best-effort conversion of the input string to a numeric type,
	// setting |*output| to the result of the conversion.  Returns true for
	// "perfect" conversions; returns false in the following cases:
	//  - Overflow. |*output| will be set to the maximum value supported
	//    by the data type.
	//  - Underflow. |*output| will be set to the minimum value supported
	//    by the data type.
	//  - Trailing characters in the string after parsing the number.  |*output|
	//    will be set to the value of the number that was parsed.
	//  - Leading whitespace in the string before parsing the number. |*output| will
	//    be set to the value of the number that was parsed.
	//  - No characters parseable as a number at the beginning of the string.
	//    |*output| will be set to 0.
	//  - Empty string.  |*output| will be set to 0.
	// WARNING: Will write to |output| even when returning false.
	//          Read the comments above carefully.
	BASE_EXPORT bool StringToInt(std::string_view input, int* output);
	BASE_EXPORT bool StringToInt(std::wstring_view input, int* output);

	BASE_EXPORT bool StringToUint(std::string_view input, unsigned* output);
	BASE_EXPORT bool StringToUint(std::wstring_view input, unsigned* output);

	BASE_EXPORT bool StringToInt64(std::string_view input, int64_t* output);
	BASE_EXPORT bool StringToInt64(std::wstring_view input, int64_t* output);

	BASE_EXPORT bool StringToUint64(std::string_view input, uint64_t* output);
	BASE_EXPORT bool StringToUint64(std::wstring_view input, uint64_t* output);

	BASE_EXPORT bool StringToSizeT(std::string_view input, size_t* output);
	BASE_EXPORT bool StringToSizeT(std::wstring_view input, size_t* output);

	// For floating-point conversions, only conversions of input strings in decimal
	// form are defined to work.  Behavior with strings representing floating-point
	// numbers in hexadecimal, and strings representing non-finite values (such as
	// NaN and inf) is undefined.  Otherwise, these behave the same as the integral
	// variants.  This expects the input string to NOT be specific to the locale.
	// If your input is locale specific, use ICU to read the number.
	// WARNING: Will write to |output| even when returning false.
	//          Read the comments here and above StringToInt() carefully.
	BASE_EXPORT bool StringToDouble(std::string_view input, double* output);
	BASE_EXPORT bool StringToDouble(std::wstring_view input, double* output);

	// Hex encoding ----------------------------------------------------------------

	// Returns a hex string representation of a binary buffer. The returned hex
	// string will be in upper case. This function does not check if |size| is
	// within reasonable limits since it's written with trusted data in mind.  If
	// you suspect that the data you want to format might be large, the absolute
	// max size for |size| should be is
	//   std::numeric_limits<size_t>::max() / 2
	BASE_EXPORT std::string HexEncode(const void* bytes, size_t size);
	BASE_EXPORT std::string HexEncode(span<const uint8_t> bytes);

	// Best effort conversion, see StringToInt above for restrictions.
	// Will only successful parse hex values that will fit into |output|, i.e.
	// -0x80000000 < |input| < 0x7FFFFFFF.
	BASE_EXPORT bool HexStringToInt(std::string_view input, int* output);

	// Best effort conversion, see StringToInt above for restrictions.
	// Will only successful parse hex values that will fit into |output|, i.e.
	// 0x00000000 < |input| < 0xFFFFFFFF.
	// The string is not required to start with 0x.
	BASE_EXPORT bool HexStringToUInt(std::string_view input, uint32_t* output);

	// Best effort conversion, see StringToInt above for restrictions.
	// Will only successful parse hex values that will fit into |output|, i.e.
	// -0x8000000000000000 < |input| < 0x7FFFFFFFFFFFFFFF.
	BASE_EXPORT bool HexStringToInt64(std::string_view input, int64_t* output);

	// Best effort conversion, see StringToInt above for restrictions.
	// Will only successful parse hex values that will fit into |output|, i.e.
	// 0x0000000000000000 < |input| < 0xFFFFFFFFFFFFFFFF.
	// The string is not required to start with 0x.
	BASE_EXPORT bool HexStringToUInt64(std::string_view input, uint64_t* output);

	// Similar to the previous functions, except that output is a vector of bytes.
	// |*output| will contain as many bytes as were successfully parsed prior to the
	// error.  There is no overflow, but input.size() must be evenly divisible by 2.
	// Leading 0x or +/- are not allowed.
	BASE_EXPORT bool HexStringToBytes(std::string_view input, 
									  std::vector<uint8_t>* output);

} // namespace base