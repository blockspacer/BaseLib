// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pch.h"
#include "json/json_reader.h"

#include <utility>
#include <optional>

#include "base_paths.h"
#include "files/file_util.h"
#include "logging.h"
#include "path_service.h"
#include "stl_util.h"
#include "strings/utf_string_conversions.h"
#include "values.h"

namespace base {

	TEST(JSONReaderTest, Whitespace) {
		auto root = JSONReader::Read("   null   ");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_none());
	}

	TEST(JSONReaderTest, InvalidString) {
		EXPECT_FALSE(JSONReader::Read("nu"));
	}

	TEST(JSONReaderTest, SimpleBool) {
		std::optional<Value> root = JSONReader::Read("true  ");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_bool());
	}

	TEST(JSONReaderTest, EmbeddedComments) {
		auto root = JSONReader::Read("/* comment */null");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_none());
		root = JSONReader::Read("40 /* comment */");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_int());
		root = JSONReader::Read("true // comment");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_bool());
		root = JSONReader::Read("/* comment */\"sample string\"");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_string());
		std::string value;
		EXPECT_TRUE(root->GetAsString(&value));
		EXPECT_EQ("sample string", value);
		root = JSONReader::Read("[1, /* comment, 2 ] */ \n 3]");
		ASSERT_TRUE(root);
		ASSERT_TRUE(root->is_list());
		ASSERT_EQ(2u, root->GetList().size());
		ASSERT_TRUE(root->GetList()[0].is_int());
		EXPECT_EQ(1, root->GetList()[0].GetInt());
		ASSERT_TRUE(root->GetList()[1].is_int());
		EXPECT_EQ(3, root->GetList()[1].GetInt());
		root = JSONReader::Read("[1, /*a*/2, 3]");
		ASSERT_TRUE(root);
		ASSERT_TRUE(root->is_list());
		EXPECT_EQ(3u, root->GetList().size());
		root = JSONReader::Read("/* comment **/42");
		ASSERT_TRUE(root);
		ASSERT_TRUE(root->is_int());
		EXPECT_EQ(42, root->GetInt());
		root = JSONReader::Read(
			"/* comment **/\n"
			"// */ 43\n"
			"44");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_int());
		EXPECT_EQ(44, root->GetInt());
	}

	TEST(JSONReaderTest, Ints) {
		std::optional<Value> root = JSONReader::Read("43");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_int());
		int int_val = 0;
		EXPECT_TRUE(root->GetAsInteger(&int_val));
		EXPECT_EQ(43, int_val);
	}

	TEST(JSONReaderTest, NonDecimalNumbers) {
		// According to RFC4627, oct, hex, and leading zeros are invalid JSON.
		EXPECT_FALSE(JSONReader::Read("043"));
		EXPECT_FALSE(JSONReader::Read("0x43"));
		EXPECT_FALSE(JSONReader::Read("00"));
	}

	TEST(JSONReaderTest, NumberZero) {
		// Test 0 (which needs to be special cased because of the leading zero
		// clause).
		std::optional<Value> root = JSONReader::Read("0");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_int());
		int int_val = 1;
		EXPECT_TRUE(root->GetAsInteger(&int_val));
		EXPECT_EQ(0, int_val);
	}

	TEST(JSONReaderTest, LargeIntPromotion) {
		// Numbers that overflow ints should succeed, being internally promoted to
		// storage as doubles
		std::optional<Value> root = JSONReader::Read("2147483648");
		ASSERT_TRUE(root);
		double double_val;
		EXPECT_TRUE(root->is_double());
		double_val = 0.0;
		EXPECT_TRUE(root->GetAsDouble(&double_val));
		EXPECT_DOUBLE_EQ(2147483648.0, double_val);
		root = JSONReader::Read("-2147483649");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_double());
		double_val = 0.0;
		EXPECT_TRUE(root->GetAsDouble(&double_val));
		EXPECT_DOUBLE_EQ(-2147483649.0, double_val);
	}

	TEST(JSONReaderTest, Doubles) {
		std::optional<Value> root = JSONReader::Read("43.1");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_double());
		double double_val = 0.0;
		EXPECT_TRUE(root->GetAsDouble(&double_val));
		EXPECT_DOUBLE_EQ(43.1, double_val);

		root = JSONReader::Read("4.3e-1");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_double());
		double_val = 0.0;
		EXPECT_TRUE(root->GetAsDouble(&double_val));
		EXPECT_DOUBLE_EQ(.43, double_val);

		root = JSONReader::Read("2.1e0");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_double());
		double_val = 0.0;
		EXPECT_TRUE(root->GetAsDouble(&double_val));
		EXPECT_DOUBLE_EQ(2.1, double_val);

		root = JSONReader::Read("2.1e+0001");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_double());
		double_val = 0.0;
		EXPECT_TRUE(root->GetAsDouble(&double_val));
		EXPECT_DOUBLE_EQ(21.0, double_val);

		root = JSONReader::Read("0.01");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_double());
		double_val = 0.0;
		EXPECT_TRUE(root->GetAsDouble(&double_val));
		EXPECT_DOUBLE_EQ(0.01, double_val);

		root = JSONReader::Read("1.00");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_double());
		double_val = 0.0;
		EXPECT_TRUE(root->GetAsDouble(&double_val));
		EXPECT_DOUBLE_EQ(1.0, double_val);
	}

	TEST(JSONReaderTest, FractionalNumbers) {
		// Fractional parts must have a digit before and after the decimal point.
		EXPECT_FALSE(JSONReader::Read("1."));
		EXPECT_FALSE(JSONReader::Read(".1"));
		EXPECT_FALSE(JSONReader::Read("1.e10"));
	}

	TEST(JSONReaderTest, ExponentialNumbers) {
		// Exponent must have a digit following the 'e'.
		EXPECT_FALSE(JSONReader::Read("1e"));
		EXPECT_FALSE(JSONReader::Read("1E"));
		EXPECT_FALSE(JSONReader::Read("1e1."));
		EXPECT_FALSE(JSONReader::Read("1e1.0"));
	}

	TEST(JSONReaderTest, InvalidNAN) {
		EXPECT_FALSE(JSONReader::Read("1e1000"));
		EXPECT_FALSE(JSONReader::Read("-1e1000"));
		EXPECT_FALSE(JSONReader::Read("NaN"));
		EXPECT_FALSE(JSONReader::Read("nan"));
		EXPECT_FALSE(JSONReader::Read("inf"));
	}

	TEST(JSONReaderTest, InvalidNumbers) {
		EXPECT_FALSE(JSONReader::Read("4.3.1"));
		EXPECT_FALSE(JSONReader::Read("4e3.1"));
		EXPECT_FALSE(JSONReader::Read("4.a"));
	}

	TEST(JSONReader, SimpleString) {
		std::optional<Value> root = JSONReader::Read("\"hello world\"");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_string());
		std::string str_val;
		EXPECT_TRUE(root->GetAsString(&str_val));
		EXPECT_EQ("hello world", str_val);
	}

	TEST(JSONReaderTest, EmptyString) {
		std::optional<Value> root = JSONReader::Read("\"\"");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_string());
		std::string str_val;
		EXPECT_TRUE(root->GetAsString(&str_val));
		EXPECT_EQ("", str_val);
	}

	TEST(JSONReaderTest, BasicStringEscapes) {
		auto root = JSONReader::Read(R"(" \"\\\/\b\f\n\r\t\v")");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_string());
		std::string str_val;
		EXPECT_TRUE(root->GetAsString(&str_val));
		EXPECT_EQ(" \"\\/\b\f\n\r\t\v", str_val);
	}

	TEST(JSONReaderTest, UnicodeEscapes) {
		// Test hex and unicode escapes including the null character.
		auto root = JSONReader::Read(R"("\x41\x00\u1234\u0000")");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_string());
		std::string str_val;
		EXPECT_TRUE(root->GetAsString(&str_val));
		EXPECT_EQ(std::wstring(L"A\0\x1234\0", 4), UTF8ToWide(str_val));
	}

	TEST(JSONReaderTest, InvalidStrings) {
		EXPECT_FALSE(JSONReader::Read("\"no closing quote"));
		EXPECT_FALSE(JSONReader::Read("\"\\z invalid escape char\""));
		EXPECT_FALSE(JSONReader::Read("\"\\xAQ invalid hex code\""));
		EXPECT_FALSE(JSONReader::Read("not enough hex chars\\x1\""));
		EXPECT_FALSE(JSONReader::Read("\"not enough escape chars\\u123\""));
		EXPECT_FALSE(JSONReader::Read("\"extra backslash at end of input\\\""));
	}

	TEST(JSONReaderTest, BasicArray) {
		auto list = JSONReader::Read("[true, false, null]");
		ASSERT_TRUE(list);
		ASSERT_TRUE(list->is_list());
		EXPECT_EQ(3U, list->GetList().size());

		// Test with trailing comma.  Should be parsed the same as above.
		auto root2 =
			JSONReader::Read("[true, false, null, ]", JSON_ALLOW_TRAILING_COMMAS);
		ASSERT_TRUE(root2);
		EXPECT_EQ(*list, *root2);
	}

	TEST(JSONReaderTest, EmptyArray) {
		std::optional<Value> list = JSONReader::Read("[]");
		ASSERT_TRUE(list);
		ASSERT_TRUE(list->is_list());
		EXPECT_TRUE(list->GetList().empty());
	}

	TEST(JSONReaderTest, NestedArrays) {
		auto list =
			JSONReader::Read("[[true], [], [false, [], [null]], null]");
		ASSERT_TRUE(list);
		ASSERT_TRUE(list->is_list());
		EXPECT_EQ(4U, list->GetList().size());

		// Lots of trailing commas.
		auto root2 =
			JSONReader::Read("[[true], [], [false, [], [null, ]  , ], null,]",
				JSON_ALLOW_TRAILING_COMMAS);
		ASSERT_TRUE(root2);
		EXPECT_EQ(*list, *root2);
	}

	TEST(JSONReaderTest, InvalidArrays) {
		// Missing close brace.
		EXPECT_FALSE(JSONReader::Read("[[true], [], [false, [], [null]], null"));

		// Too many commas.
		EXPECT_FALSE(JSONReader::Read("[true,, null]"));
		EXPECT_FALSE(JSONReader::Read("[true,, null]", JSON_ALLOW_TRAILING_COMMAS));

		// No commas.
		EXPECT_FALSE(JSONReader::Read("[true null]"));

		// Trailing comma.
		EXPECT_FALSE(JSONReader::Read("[true,]"));
	}

	TEST(JSONReaderTest, ArrayTrailingComma) {
		// Valid if we set |allow_trailing_comma| to true.
		auto list =
			JSONReader::Read("[true,]", JSON_ALLOW_TRAILING_COMMAS);
		ASSERT_TRUE(list);
		ASSERT_TRUE(list->is_list());
		ASSERT_EQ(1U, list->GetList().size());
		const auto& value1 = list->GetList()[0];
		ASSERT_TRUE(value1.is_bool());
		EXPECT_TRUE(value1.GetBool());
	}

	TEST(JSONReaderTest, ArrayTrailingCommaNoEmptyElements) {
		// Don't allow empty elements, even if |allow_trailing_comma| is
		// true.
		EXPECT_FALSE(JSONReader::Read("[,]", JSON_ALLOW_TRAILING_COMMAS));
		EXPECT_FALSE(JSONReader::Read("[true,,]", JSON_ALLOW_TRAILING_COMMAS));
		EXPECT_FALSE(JSONReader::Read("[,true,]", JSON_ALLOW_TRAILING_COMMAS));
		EXPECT_FALSE(JSONReader::Read("[true,,false]", JSON_ALLOW_TRAILING_COMMAS));
	}

	TEST(JSONReaderTest, EmptyDictionary) {
		std::optional<Value> dict_val = JSONReader::Read("{}");
		ASSERT_TRUE(dict_val);
		ASSERT_TRUE(dict_val->is_dict());
	}

	TEST(JSONReaderTest, CompleteDictionary) {
		auto dict_val = JSONReader::Read(
			R"({"number":9.87654321, "null":null , "\x53" : "str" })");
		ASSERT_TRUE(dict_val);
		ASSERT_TRUE(dict_val->is_dict());
		auto double_val = dict_val->FindDoubleKey("number");
		ASSERT_TRUE(double_val);
		EXPECT_DOUBLE_EQ(9.87654321, *double_val);
		const Value* null_val =
			dict_val->FindKeyOfType("null", base::Value::Type::NONE);
		ASSERT_TRUE(null_val);
		EXPECT_TRUE(null_val->is_none());
		auto str_val = dict_val->FindStringKey("S");
		ASSERT_TRUE(str_val);
		EXPECT_EQ("str", *str_val);

		auto root2 = JSONReader::Read(
			R"({"number":9.87654321, "null":null , "\x53" : "str", })",
			JSON_ALLOW_TRAILING_COMMAS);
		ASSERT_TRUE(root2);
		ASSERT_TRUE(root2->is_dict());
		EXPECT_EQ(*dict_val, *root2);

		// Test newline equivalence.
		root2 = JSONReader::Read(
			"{\n"
			"  \"number\":9.87654321,\n"
			"  \"null\":null,\n"
			"  \"\\x53\":\"str\",\n"
			"}\n",
			JSON_ALLOW_TRAILING_COMMAS);
		ASSERT_TRUE(root2);
		ASSERT_TRUE(root2->is_dict());
		EXPECT_EQ(*dict_val, *root2);

		root2 = JSONReader::Read(
			"{\r\n"
			"  \"number\":9.87654321,\r\n"
			"  \"null\":null,\r\n"
			"  \"\\x53\":\"str\",\r\n"
			"}\r\n",
			JSON_ALLOW_TRAILING_COMMAS);
		ASSERT_TRUE(root2);
		ASSERT_TRUE(root2->is_dict());
		EXPECT_EQ(*dict_val, *root2);
	}

	TEST(JSONReaderTest, NestedDictionaries) {
		auto dict_val = JSONReader::Read(
			R"({"inner":{"array":[true]},"false":false,"d":{}})");
		ASSERT_TRUE(dict_val);
		ASSERT_TRUE(dict_val->is_dict());
		const Value* inner_dict = dict_val->FindDictKey("inner");
		ASSERT_TRUE(inner_dict);
		auto inner_array = inner_dict->FindListKey("array");
		ASSERT_TRUE(inner_array);
		EXPECT_EQ(1U, inner_array->GetList().size());
		auto bool_value = dict_val->FindBoolKey("false");
		ASSERT_TRUE(bool_value);
		EXPECT_FALSE(*bool_value);
		inner_dict = dict_val->FindDictKey("d");
		EXPECT_TRUE(inner_dict);

		std::optional<Value> root2 = JSONReader::Read(
			R"({"inner": {"array":[true] , },"false":false,"d":{},})",
			JSON_ALLOW_TRAILING_COMMAS);
		ASSERT_TRUE(root2);
		EXPECT_EQ(*dict_val, *root2);
	}

	TEST(JSONReaderTest, DictionaryKeysWithPeriods) {
		auto dict_val =
			JSONReader::Read(R"({"a.b":3,"c":2,"d.e.f":{"g.h.i.j":1}})");
		ASSERT_TRUE(dict_val);
		ASSERT_TRUE(dict_val->is_dict());

		auto integer_value = dict_val->FindIntKey("a.b");
		ASSERT_TRUE(integer_value);
		EXPECT_EQ(3, *integer_value);
		integer_value = dict_val->FindIntKey("c");
		ASSERT_TRUE(integer_value);
		EXPECT_EQ(2, *integer_value);
		const Value* inner_dict = dict_val->FindDictKey("d.e.f");
		ASSERT_TRUE(inner_dict);
		EXPECT_EQ(1U, inner_dict->DictSize());
		integer_value = inner_dict->FindIntKey("g.h.i.j");
		ASSERT_TRUE(integer_value);
		EXPECT_EQ(1, *integer_value);

		dict_val = JSONReader::Read(R"({"a":{"b":2},"a.b":1})");
		ASSERT_TRUE(dict_val->is_dict());
		const Value* integer_path_value =
			dict_val->FindPathOfType({ "a", "b" }, base::Value::Type::INTEGER);
		ASSERT_TRUE(integer_path_value);
		EXPECT_EQ(2, integer_path_value->GetInt());
		integer_value = dict_val->FindIntKey("a.b");
		ASSERT_TRUE(integer_value);
		EXPECT_EQ(1, *integer_value);
	}

	TEST(JSONReaderTest, InvalidDictionaries) {
		// No closing brace.
		EXPECT_FALSE(JSONReader::Read("{\"a\": true"));

		// Keys must be quoted strings.
		EXPECT_FALSE(JSONReader::Read("{foo:true}"));
		EXPECT_FALSE(JSONReader::Read("{1234: false}"));
		EXPECT_FALSE(JSONReader::Read("{:false}"));

		// Trailing comma.
		EXPECT_FALSE(JSONReader::Read("{\"a\":true,}"));

		// Too many commas.
		EXPECT_FALSE(JSONReader::Read("{\"a\":true,,\"b\":false}"));
		EXPECT_FALSE(JSONReader::Read("{\"a\":true,,\"b\":false}",
			JSON_ALLOW_TRAILING_COMMAS));

		// No separator.
		EXPECT_FALSE(JSONReader::Read("{\"a\" \"b\"}"));

		// Lone comma.
		EXPECT_FALSE(JSONReader::Read("{,}"));
		EXPECT_FALSE(JSONReader::Read("{,}", JSON_ALLOW_TRAILING_COMMAS));
		EXPECT_FALSE(JSONReader::Read("{\"a\":true,,}", JSON_ALLOW_TRAILING_COMMAS));
		EXPECT_FALSE(JSONReader::Read("{,\"a\":true}", JSON_ALLOW_TRAILING_COMMAS));
		EXPECT_FALSE(JSONReader::Read("{\"a\":true,,\"b\":false}",
			JSON_ALLOW_TRAILING_COMMAS));
	}

	TEST(JSONReaderTest, StackOverflow) {
		std::string evil(1000000, '[');
		evil.append(std::string(1000000, ']'));
		EXPECT_FALSE(JSONReader::Read(evil));

		// A few thousand adjacent lists is fine.
		std::string not_evil("[");
		not_evil.reserve(15010);
		for (auto i = 0; i < 5000; ++i)
			not_evil.append("[],");
		not_evil.append("[]]");
		auto list = JSONReader::Read(not_evil);
		ASSERT_TRUE(list);
		ASSERT_TRUE(list->is_list());
		EXPECT_EQ(5001U, list->GetList().size());
	}

	TEST(JSONReaderTest, UTF8Input) {
		std::optional<Value> root = JSONReader::Read("\"\xe7\xbd\x91\xe9\xa1\xb5\"");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_string());
		std::string str_val;
		EXPECT_TRUE(root->GetAsString(&str_val));
		EXPECT_EQ(L"\x7f51\x9875", UTF8ToWide(str_val));

		root = JSONReader::Read("{\"path\": \"/tmp/\xc3\xa0\xc3\xa8\xc3\xb2.png\"}");
		ASSERT_TRUE(root);
		ASSERT_TRUE(root->is_dict());
		const std::string* maybe_string = root->FindStringKey("path");
		ASSERT_TRUE(maybe_string);
		EXPECT_EQ("/tmp/\xC3\xA0\xC3\xA8\xC3\xB2.png", *maybe_string);
	}

	TEST(JSONReaderTest, InvalidUTF8Input) {
		EXPECT_FALSE(JSONReader::Read("\"345\xb0\xa1\xb0\xa2\""));
		EXPECT_FALSE(JSONReader::Read("\"123\xc0\x81\""));
		EXPECT_FALSE(JSONReader::Read("\"abc\xc0\xae\""));
	}

	TEST(JSONReaderTest, UTF16Escapes) {
		std::optional<Value> root = JSONReader::Read(R"("\u20ac3,14")");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_string());
		std::string str_val;
		EXPECT_TRUE(root->GetAsString(&str_val));
		EXPECT_EQ(
			"\xe2\x82\xac"
			"3,14",
			str_val);

		root = JSONReader::Read(R"("\ud83d\udca9\ud83d\udc6c")");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_string());
		str_val.clear();
		EXPECT_TRUE(root->GetAsString(&str_val));
		EXPECT_EQ("\xf0\x9f\x92\xa9\xf0\x9f\x91\xac", str_val);
	}

	TEST(JSONReaderTest, InvalidUTF16Escapes) {
		const char* const cases[] = {
			R"("\u123")",          // Invalid scalar.
			R"("\ud83d")",         // Invalid scalar.
			R"("\u$%@!")",         // Invalid scalar.
			R"("\uzz89")",         // Invalid scalar.
			R"("\ud83d\udca")",   // Invalid lower surrogate.
			R"("\ud83d\ud83d")",  // Invalid lower surrogate.
			"\"\\ud83d\\uaaaZ\""   // Invalid lower surrogate.
			"\"\\ud83foo\"",       // No lower surrogate.
			"\"\\ud83d\\foo\""     // No lower surrogate.
			"\"\\ud83\\foo\""      // Invalid upper surrogate.
			"\"\\ud83d\\u1\""      // No lower surrogate.
			"\"\\ud83\\u1\""       // Invalid upper surrogate.
		};
		std::optional<Value> root;
		for (auto* i : cases) {
			root = JSONReader::Read(i);
			EXPECT_FALSE(root) << i;
		}
	}

	TEST(JSONReaderTest, LiteralRoots) {
		auto root = JSONReader::Read("null");
		ASSERT_TRUE(root);
		EXPECT_TRUE(root->is_none());

		root = JSONReader::Read("true");
		ASSERT_TRUE(root);
		ASSERT_TRUE(root->is_bool());
		EXPECT_TRUE(root->GetBool());

		root = JSONReader::Read("10");
		ASSERT_TRUE(root);
		ASSERT_TRUE(root->is_int());
		EXPECT_EQ(10, root->GetInt());

		root = JSONReader::Read("\"root\"");
		ASSERT_TRUE(root);
		ASSERT_TRUE(root->is_string());
		EXPECT_EQ("root", root->GetString());
	}

	TEST(JSONReaderTest, ReadFromFile) {
		FilePath path;
		ASSERT_TRUE(PathService::Get(base::DIR_TEST_DATA, &path));
		path = path.AppendASCII("json");
		ASSERT_TRUE(base::PathExists(path));

		std::string input;
		ASSERT_TRUE(ReadFileToString(path.AppendASCII("bom_feff.json"), &input));

		JSONReader reader;
		std::optional<Value> root(reader.ReadToValue(input));
		ASSERT_TRUE(root) << reader.GetErrorMessage();
		EXPECT_TRUE(root->is_dict());
	}

	// Tests that the root of a JSON object can be deleted safely while its
	// children outlive it.
	TEST(JSONReaderTest, StringOptimizations) {
		Value dict_literal_0;
		Value dict_literal_1;
		Value dict_string_0;
		Value dict_string_1;
		Value list_value_0;
		Value list_value_1;

		{
			std::optional<Value> root = JSONReader::Read(
				"{"
				"  \"test\": {"
				"    \"foo\": true,"
				"    \"bar\": 3.14,"
				"    \"baz\": \"bat\","
				"    \"moo\": \"cow\""
				"  },"
				"  \"list\": ["
				"    \"a\","
				"    \"b\""
				"  ]"
				"}",
				JSON_PARSE_RFC);
			ASSERT_TRUE(root);
			ASSERT_TRUE(root->is_dict());

			Value* dict = root->FindDictKey("test");
			ASSERT_TRUE(dict);
			Value* list = root->FindListKey("list");
			ASSERT_TRUE(list);

			Value* to_move = dict->FindKey("foo");
			ASSERT_TRUE(to_move);
			dict_literal_0 = std::move(*to_move);
			to_move = dict->FindKey("bar");
			ASSERT_TRUE(to_move);
			dict_literal_1 = std::move(*to_move);
			to_move = dict->FindKey("baz");
			ASSERT_TRUE(to_move);
			dict_string_0 = std::move(*to_move);
			to_move = dict->FindKey("moo");
			ASSERT_TRUE(to_move);
			dict_string_1 = std::move(*to_move);
			ASSERT_TRUE(dict->RemoveKey("foo"));
			ASSERT_TRUE(dict->RemoveKey("bar"));
			ASSERT_TRUE(dict->RemoveKey("baz"));
			ASSERT_TRUE(dict->RemoveKey("moo"));

			ASSERT_EQ(2u, list->GetList().size());
			list_value_0 = std::move(list->GetList()[0]);
			list_value_1 = std::move(list->GetList()[1]);
			list->GetList().clear();
		}

		ASSERT_TRUE(dict_literal_0.is_bool());
		EXPECT_TRUE(dict_literal_0.GetBool());

		ASSERT_TRUE(dict_literal_1.is_double());
		EXPECT_EQ(3.14, dict_literal_1.GetDouble());

		ASSERT_TRUE(dict_string_0.is_string());
		EXPECT_EQ("bat", dict_string_0.GetString());

		ASSERT_TRUE(dict_string_1.is_string());
		EXPECT_EQ("cow", dict_string_1.GetString());

		ASSERT_TRUE(list_value_0.is_string());
		EXPECT_EQ("a", list_value_0.GetString());
		ASSERT_TRUE(list_value_1.is_string());
		EXPECT_EQ("b", list_value_1.GetString());
	}

	// A smattering of invalid JSON designed to test specific portions of the
	// parser implementation against buffer overflow. Best run with DCHECKs so
	// that the one in NextChar fires.
	TEST(JSONReaderTest, InvalidSanity) {
		const char* const kInvalidJson[] = {
			"/* test *", "{\"foo\"", "{\"foo\":", "  [", "\"\\u123g\"", "{\n\"eh:\n}",
		};

		for (size_t i = 0; i < base::size(kInvalidJson); ++i) {
			JSONReader reader;
			LOG(INFO) << "Sanity test " << i << ": <" << kInvalidJson[i] << ">";
			EXPECT_FALSE(reader.ReadToValue(kInvalidJson[i]));
			EXPECT_NE(JSONReader::JSON_NO_ERROR, reader.error_code());
			EXPECT_NE("", reader.GetErrorMessage());
		}
	}

	TEST(JSONReaderTest, IllegalTrailingNull) {
		const char json[] = { '"', 'n', 'u', 'l', 'l', '"', '\0' };
		std::string json_string(json, sizeof(json));
		JSONReader reader;
		EXPECT_FALSE(reader.ReadToValue(json_string));
		EXPECT_EQ(JSONReader::JSON_UNEXPECTED_DATA_AFTER_ROOT, reader.error_code());
	}

	TEST(JSONReaderTest, MaxNesting) {
		std::string json(R"({"outer": { "inner": {"foo": true}}})");
		EXPECT_FALSE(JSONReader::Read(json, JSON_PARSE_RFC, 3));
		EXPECT_TRUE(JSONReader::Read(json, JSON_PARSE_RFC, 4));
	}

}  // namespace base
