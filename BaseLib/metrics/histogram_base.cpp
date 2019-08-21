// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "metrics/histogram_base.h"

#include <climits>

#include <memory>
#include <set>
#include <utility>

#include "json/json_string_value_serializer.h"
#include "logging.h"
#include "metrics/histogram.h"
#include "metrics/histogram_samples.h"
#include "metrics/sparse_histogram.h"
#include "metrics/statistics_recorder.h"
#include "no_destructor.h"
#include "numerics/safe_conversions.h"
#include "pickle.h"
#include "process/process_handle.h"
#include "rand_util.h"
#include "strings/stringprintf.h"
#include "synchronization/lock.h"
#include "values.h"

namespace base {

	std::string HistogramTypeToString(HistogramType type) {
		switch (type) {
		case HISTOGRAM:
			return "HISTOGRAM";
		case LINEAR_HISTOGRAM:
			return "LINEAR_HISTOGRAM";
		case BOOLEAN_HISTOGRAM:
			return "BOOLEAN_HISTOGRAM";
		case CUSTOM_HISTOGRAM:
			return "CUSTOM_HISTOGRAM";
		case SPARSE_HISTOGRAM:
			return "SPARSE_HISTOGRAM";
		case DUMMY_HISTOGRAM:
			return "DUMMY_HISTOGRAM";
		}
		NOTREACHED();
		return "UNKNOWN";
	}

	HistogramBase* DeserializeHistogramInfo(PickleIterator* iter) {
		int type;
		if (!iter->ReadInt(&type))
			return nullptr;

		switch (type) {
		case HISTOGRAM:
			return Histogram::DeserializeInfoImpl(iter);
		case LINEAR_HISTOGRAM:
			return LinearHistogram::DeserializeInfoImpl(iter);
		case BOOLEAN_HISTOGRAM:
			return BooleanHistogram::DeserializeInfoImpl(iter);
		case CUSTOM_HISTOGRAM:
			return CustomHistogram::DeserializeInfoImpl(iter);
		case SPARSE_HISTOGRAM:
			return SparseHistogram::DeserializeInfoImpl(iter);
		default:
			return nullptr;
		}
	}

	const HistogramBase::Sample HistogramBase::kSampleType_MAX = INT_MAX;

	HistogramBase::HistogramBase(const char* name) 
		: histogram_name_(name), flags_(kNoFlags) {}

	HistogramBase::~HistogramBase() = default;

	void HistogramBase::CheckName(const std::string_view& name) const {
		DCHECK_EQ(histogram_name(), name.data());
	}

	void HistogramBase::SetFlags(int32_t flags) {
		const auto old_flags = subtle::NoBarrier_Load(&flags_);
		subtle::NoBarrier_Store(&flags_, old_flags | flags);
	}

	void HistogramBase::ClearFlags(int32_t flags) {
		const auto old_flags = subtle::NoBarrier_Load(&flags_);
		subtle::NoBarrier_Store(&flags_, old_flags & ~flags);
	}

	void HistogramBase::AddScaled(Sample value, int count, int scale) {
		DCHECK_LT(0, scale);

		// Convert raw count and probabilistically round up/down if the remainder
		// is more than a random number [0, scale). This gives a more accurate
		// count when there are a large number of records. RandInt is "inclusive",
		// hence the -1 for the max value.
		int64_t count_scaled = count / scale;
		if (count - (count_scaled * scale) > base::RandInt(0, scale - 1))
			count_scaled += 1;
		if (count_scaled == 0)
			return;

		AddCount(value, static_cast<int>(count_scaled));
	}

	void HistogramBase::AddKilo(Sample value, int count) {
		AddScaled(value, count, 1000);
	}

	void HistogramBase::AddKiB(Sample value, int count) {
		AddScaled(value, count, 1024);
	}

	void HistogramBase::AddTimeMillisecondsGranularity(const TimeDelta& time) {
		Add(saturated_cast<Sample>(time.InMilliseconds()));
	}

	void HistogramBase::AddTimeMicrosecondsGranularity(const TimeDelta& time) {
		// Intentionally drop high-resolution reports on clients with low-resolution
		// clocks. High-resolution metrics cannot make use of low-resolution data and
		// reporting it merely adds noise to the metric. https://crbug.com/807615#c16
		if (TimeTicks::IsHighResolution())
			Add(saturated_cast<Sample>(time.InMicroseconds()));
	}

	void HistogramBase::AddBoolean(bool value) {
		Add(value ? 1 : 0);
	}

	void HistogramBase::SerializeInfo(Pickle* pickle) const {
		pickle->WriteInt(GetHistogramType());
		SerializeInfoImpl(pickle);
	}

	uint32_t HistogramBase::FindCorruption(const HistogramSamples& samples) const {
		// Not supported by default.
		return NO_INCONSISTENCIES;
	}

	void HistogramBase::ValidateHistogramContents() const {}

	void HistogramBase::WriteJSON(std::string* output, 
								  JSONVerbosityLevel verbosity_level) const {
		Count count = 0;
		int64_t sum = 0;
		std::unique_ptr<ListValue> buckets(new ListValue());
		GetCountAndBucketData(&count, &sum, buckets.get());
		std::unique_ptr<DictionaryValue> parameters(new DictionaryValue());
		GetParameters(parameters.get());

		JSONStringValueSerializer serializer(output);
		DictionaryValue root;
		root.SetStringKey("name", histogram_name());
		root.SetIntKey("count", count);
		root.SetDoubleKey("sum", static_cast<double>(sum));
		root.SetIntKey("flags", flags());
		root.Set("params", std::move(parameters));
		if (verbosity_level != JSON_VERBOSITY_LEVEL_OMIT_BUCKETS)
			root.Set("buckets", std::move(buckets));
		root.SetIntKey("pid", GetUniqueIdForProcess().GetUnsafeValue());
		serializer.Serialize(root);
	}

	void HistogramBase::FindAndRunCallback(HistogramBase::Sample sample) const {
		if ((flags() & kCallbackExists) == 0)
			return;

		const auto cb = 
			StatisticsRecorder::FindCallback(histogram_name());
		if (!cb.is_null())
			cb.Run(sample);
	}

	void HistogramBase::WriteAsciiBucketGraph(double current_size, 
											  double max_size, 
											  std::string* output) const {
		const auto k_line_length = 72;  // Maximal horizontal width of graph.
		auto x_count = lround(k_line_length * (current_size / max_size));
		auto x_remainder = k_line_length - x_count;

		while (0 < x_count--)
			output->append("-");
		output->append("O");
		while (0 < x_remainder--)
			output->append(" ");
	}

	std::string HistogramBase::GetSimpleAsciiBucketRange(
		Sample sample) const
	{
		return StringPrintf("%d", sample);
	}

	void HistogramBase::WriteAsciiBucketValue(Count current, 
											  double scaled_sum, 
											  std::string* output) const {
		StringAppendF(output, " (%d = %3.1f%%)", current, current / scaled_sum);
	}

	// static
	char const* HistogramBase::GetPermanentName(const std::string& name) {
		// A set of histogram names that provides the "permanent" lifetime required
		// by histogram objects for those strings that are not already code constants
		// or held in persistent memory.
		static NoDestructor<std::set<std::string>> permanent_names;
		static NoDestructor<Lock> permanent_names_lock;

		AutoLock lock(*permanent_names_lock);
		const auto result = permanent_names->insert(name);
		return result.first->c_str();
	}

}  // namespace base