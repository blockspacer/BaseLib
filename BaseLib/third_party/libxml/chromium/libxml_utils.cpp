// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libxml_utils.h"

#include <vector>

#include "libxml/xmlreader.h"

namespace {

	// Converts a libxml xmlChar* into a UTF-8 std::string.
	// NULL inputs produce an empty string.
	std::string XmlStringToStdString(const xmlChar* xmlstring) {
		if (!xmlstring)
			return std::string();

		// xmlChar*s are UTF-8, so this cast is safe.
		return std::string(reinterpret_cast<const char*>(xmlstring));
	}

	// Same as XmlStringToStdString but also frees |xmlstring|.
	std::string XmlStringToStdStringWithDelete(xmlChar* xmlstring) {
		std::string result = XmlStringToStdString(xmlstring);
		xmlFree(xmlstring);
		return result;
	}

	enum GetAttributesQueryType { ATTRIBUTES, NAMESPACES_PREFIXES };

	// Populates |names| with the names of the attributes or prefix of namespaces
	// (depending on |query_type|) for the current node in |reader|.
	// Returns true if attribute names/namespace prefixes were retrieved, false
	// otherwise.
	// Note the strings in |names| are valid as long as |reader| is valid and should
	// not be deleted.
	bool GetNodeAttributeNames(xmlTextReaderPtr reader,
		GetAttributesQueryType query_type,
		std::vector<const xmlChar*>* names) {
		if (xmlTextReaderHasAttributes(reader) <= 0)
			return false;

		if (!xmlTextReaderMoveToFirstAttribute(reader))
			return false;

		do {
			bool is_namespace = xmlTextReaderIsNamespaceDecl(reader) == 1;
			if (query_type == NAMESPACES_PREFIXES && is_namespace) {
				// Use the local name for namespaces so we don't include 'xmlns:".
				names->push_back(xmlTextReaderConstLocalName(reader));
			}
			else if (query_type == ATTRIBUTES && !is_namespace) {
				// Use the fully qualified name for attributes.
				names->push_back(xmlTextReaderConstName(reader));
			}
		} while (xmlTextReaderMoveToNextAttribute(reader) > 0);

		// Move the reader from the attributes back to the containing element.
		if (!xmlTextReaderMoveToElement(reader))
			return false;

		return true;
	}

}  // namespace

XmlReader::XmlReader() : reader_(NULL) {
}

XmlReader::~XmlReader() {
	if (reader_)
		xmlFreeTextReader(reader_);
}

bool XmlReader::Load(const std::string& input) {
	const int kParseOptions = XML_PARSE_NONET;  // forbid network access
	// TODO(evanm): Verify it's OK to pass NULL for the URL and encoding.
	// The libxml code allows for these, but it's unclear what effect is has.
	reader_ = xmlReaderForMemory(input.data(), static_cast<int>(input.size()),
		NULL, NULL, kParseOptions);
	return reader_ != NULL;
}

bool XmlReader::LoadFile(const std::string& file_path) {
	const int kParseOptions = XML_PARSE_NONET;  // forbid network access
	reader_ = xmlReaderForFile(file_path.c_str(), NULL, kParseOptions);
	return reader_ != NULL;
}

std::string XmlReader::NodeName() {
	return XmlStringToStdString(xmlTextReaderConstLocalName(reader_));
}

std::string XmlReader::NodeFullName() {
	return XmlStringToStdString(xmlTextReaderConstName(reader_));
}

bool XmlReader::NodeAttribute(const char* name, std::string* out) {
	xmlChar* value = xmlTextReaderGetAttribute(reader_, BAD_CAST name);
	if (!value)
		return false;
	*out = XmlStringToStdStringWithDelete(value);
	return true;
}

bool XmlReader::GetAllNodeAttributes(
	std::map<std::string, std::string>* attributes) {
	std::vector<const xmlChar*> attribute_names;
	if (!GetNodeAttributeNames(reader_, ATTRIBUTES, &attribute_names))
		return false;

	// Retrieve the attribute values.
	for (const auto* name : attribute_names) {
		(*attributes)[XmlStringToStdString(name)] = XmlStringToStdStringWithDelete(
			xmlTextReaderGetAttribute(reader_, name));
	}
	return true;
}

bool XmlReader::GetAllDeclaredNamespaces(
	std::map<std::string, std::string>* namespaces) {
	std::vector<const xmlChar*> prefixes;
	if (!GetNodeAttributeNames(reader_, NAMESPACES_PREFIXES, &prefixes))
		return false;

	// Retrieve the namespace URIs.
	for (const auto* prefix : prefixes) {
		bool default_namespace = xmlStrcmp(prefix, BAD_CAST "xmlns") == 0;

		std::string value = XmlStringToStdStringWithDelete(
			xmlTextReaderLookupNamespace(reader_, prefix));
		if (value.empty() && default_namespace) {
			// Default namespace is treated as an attribute for some reason.
			value = XmlStringToStdStringWithDelete(
				xmlTextReaderGetAttribute(reader_, prefix));
		}
		(*namespaces)[default_namespace ? "" : XmlStringToStdString(prefix)] =
			value;
	}
	return true;
}

bool XmlReader::GetTextIfTextElement(std::string* content) {
	if (NodeType() != XML_READER_TYPE_TEXT)
		return false;

	*content = XmlStringToStdString(xmlTextReaderConstValue(reader_));
	return true;
}

bool XmlReader::GetTextIfCDataElement(std::string* content) {
	if (NodeType() != XML_READER_TYPE_CDATA)
		return false;

	*content = XmlStringToStdString(xmlTextReaderConstValue(reader_));
	return true;
}

bool XmlReader::IsElement() {
	return NodeType() == XML_READER_TYPE_ELEMENT;
}

bool XmlReader::IsClosingElement() {
	return NodeType() == XML_READER_TYPE_END_ELEMENT;
}

bool XmlReader::IsEmptyElement() {
	return xmlTextReaderIsEmptyElement(reader_);
}

bool XmlReader::ReadElementContent(std::string* content) {
	const int start_depth = Depth();

	if (xmlTextReaderIsEmptyElement(reader_)) {
		// Empty tag.  We succesfully read the content, but it's
		// empty.
		*content = "";
		// Advance past this empty tag.
		if (!Read())
			return false;
		return true;
	}

	// Advance past opening element tag.
	if (!Read())
		return false;

	// Read the content.  We read up until we hit a closing tag at the
	// same level as our starting point.
	while (NodeType() != XML_READER_TYPE_END_ELEMENT || Depth() != start_depth) {
		*content += XmlStringToStdString(xmlTextReaderConstValue(reader_));
		if (!Read())
			return false;
	}

	// Advance past ending element tag.
	if (!Read())
		return false;

	return true;
}

bool XmlReader::SkipToElement() {
	do {
		switch (NodeType()) {
		case XML_READER_TYPE_ELEMENT:
			return true;
		case XML_READER_TYPE_END_ELEMENT:
			return false;
		default:
			// Skip all other node types.
			continue;
		}
	} while (Read());
	return false;
}


// XmlWriter functions

XmlWriter::XmlWriter()
	: writer_(NULL),
	buffer_(NULL) {}

XmlWriter::~XmlWriter() {
	if (writer_)
		xmlFreeTextWriter(writer_);
	if (buffer_)
		xmlBufferFree(buffer_);
}

void XmlWriter::StartWriting() {
	buffer_ = xmlBufferCreate();
	writer_ = xmlNewTextWriterMemory(buffer_, 0);
	xmlTextWriterSetIndent(writer_, 1);
	xmlTextWriterStartDocument(writer_, NULL, NULL, NULL);
}

void XmlWriter::StopWriting() {
	xmlTextWriterEndDocument(writer_);
	xmlFreeTextWriter(writer_);
	writer_ = NULL;
}

std::string XmlWriter::GetWrittenString() {
	return buffer_ ? XmlStringToStdString(buffer_->content) : "";
}