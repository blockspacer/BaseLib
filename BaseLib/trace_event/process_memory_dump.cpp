// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trace_event/process_memory_dump.h"

#include <vector>

#include "memory/ptr_util.h"
#include "memory/shared_memory_tracker.h"
#include "process/process_metrics.h"
#include "strings/stringprintf.h"
#include "trace_event/memory_infra_background_whitelist.h"
#include "trace_event/trace_event_impl.h"
#include "trace_event/traced_value.h"
#include "trace_event_memory_overhead.h"
#include "unguessable_token.h"
#include "build_config.h"

#include <Windows.h>  // Must be in front of other Windows header files

#include <Psapi.h>

namespace base {
	namespace trace_event {

		namespace {

			const char kEdgeTypeOwnership[] = "ownership";

			std::string GetSharedGlobalAllocatorDumpName(
				const MemoryAllocatorDumpGuid& guid) {
				return "global/" + guid.ToString();
			}

#if defined(COUNT_RESIDENT_BYTES_SUPPORTED)
			size_t GetSystemPageCount(size_t mapped_size, size_t page_size) {
				return (mapped_size + page_size - 1) / page_size;
			}
#endif

			UnguessableToken GetTokenForCurrentProcess() {
				static UnguessableToken instance = UnguessableToken::Create();
				return instance;
			}

		}  // namespace

		// static
		bool ProcessMemoryDump::is_black_hole_non_fatal_for_testing_ = false;

#if defined(COUNT_RESIDENT_BYTES_SUPPORTED)
		// static
		size_t ProcessMemoryDump::GetSystemPageSize() {
			return GetPageSize();
		}

		// static
		size_t ProcessMemoryDump::CountResidentBytes(void* start_address,
			size_t mapped_size) {
			const size_t page_size = GetSystemPageSize();
			const auto start_pointer = reinterpret_cast<uintptr_t>(start_address);
			DCHECK_EQ(0u, start_pointer % page_size);

			size_t offset = 0;
			size_t total_resident_pages = 0;
			bool failure = false;

			// An array as large as number of pages in memory segment needs to be passed
			// to the query function. To avoid allocating a large array, the given block
			// of memory is split into chunks of size |kMaxChunkSize|.
			const size_t kMaxChunkSize = 8 * 1024 * 1024;
#undef min
			const auto max_vec_size =
				GetSystemPageCount(std::min(mapped_size, kMaxChunkSize), page_size);
			const std::unique_ptr<PSAPI_WORKING_SET_EX_INFORMATION[]> vec(
				new PSAPI_WORKING_SET_EX_INFORMATION[max_vec_size]);

			while (offset < mapped_size) {
				const auto chunk_start = (start_pointer + offset);
				const auto chunk_size = std::min(mapped_size - offset, kMaxChunkSize);
				const auto page_count = GetSystemPageCount(chunk_size, page_size);
				size_t resident_page_count = 0;
				for (size_t i = 0; i < page_count; i++) {
					vec[i].VirtualAddress =
						reinterpret_cast<void*>(chunk_start + i * page_size);
				}
				const auto vec_size = static_cast<DWORD>(
					page_count * sizeof(PSAPI_WORKING_SET_EX_INFORMATION));
				failure = !QueryWorkingSetEx(GetCurrentProcess(), vec.get(), vec_size);

				for (size_t i = 0; i < page_count; i++)
					resident_page_count += vec[i].VirtualAttributes.Valid;

				if (failure)
					break;

				total_resident_pages += resident_page_count * page_size;
				offset += kMaxChunkSize;
			}

			DCHECK(!failure);
			if (failure) {
				total_resident_pages = 0;
				LOG(ERROR) << "CountResidentBytes failed. The resident size is invalid";
			}
			return total_resident_pages;
		}

		// static
		std::optional<size_t> ProcessMemoryDump::CountResidentBytesInSharedMemory(
			void* start_address, 
			size_t mapped_size) {
			return CountResidentBytes(start_address, mapped_size);
		}

#endif  // defined(COUNT_RESIDENT_BYTES_SUPPORTED)

		ProcessMemoryDump::ProcessMemoryDump(
			const MemoryDumpArgs& dump_args)
			 : process_token_(GetTokenForCurrentProcess()),
			dump_args_(dump_args) {}

		ProcessMemoryDump::~ProcessMemoryDump() = default;
		ProcessMemoryDump::ProcessMemoryDump(ProcessMemoryDump&& other) = default;
		ProcessMemoryDump& ProcessMemoryDump::operator=(ProcessMemoryDump&& other) = 
			default;

		MemoryAllocatorDump* ProcessMemoryDump::CreateAllocatorDump(
			const std::string& absolute_name) {
			return AddAllocatorDumpInternal(std::make_unique<MemoryAllocatorDump>(
				absolute_name, dump_args_.level_of_detail, GetDumpId(absolute_name)));
		}

		MemoryAllocatorDump* ProcessMemoryDump::CreateAllocatorDump(
			const std::string& absolute_name, 
			const MemoryAllocatorDumpGuid& guid) {
			return AddAllocatorDumpInternal(std::make_unique<MemoryAllocatorDump>(
				absolute_name, dump_args_.level_of_detail, guid));
		}

		MemoryAllocatorDump* ProcessMemoryDump::AddAllocatorDumpInternal(
			std::unique_ptr<MemoryAllocatorDump> mad) {
			// In background mode return the black hole dump, if invalid dump name is
			// given.
			if (dump_args_.level_of_detail == MemoryDumpLevelOfDetail::BACKGROUND &&
				!IsMemoryAllocatorDumpNameWhitelisted(mad->absolute_name())) {
				return GetBlackHoleMad();
			}

			const auto insertion_result = allocator_dumps_.insert(
				std::make_pair(mad->absolute_name(), std::move(mad)));
			MemoryAllocatorDump* inserted_mad = insertion_result.first->second.get();
			DCHECK(insertion_result.second) << "Duplicate name: " 
											<< inserted_mad->absolute_name();
			return inserted_mad;
		}

		MemoryAllocatorDump* ProcessMemoryDump::GetAllocatorDump(
			const std::string& absolute_name) const {
			const auto it = allocator_dumps_.find(absolute_name);
			if (it != allocator_dumps_.end())
				return it->second.get();
			return nullptr;
		}

		MemoryAllocatorDump* ProcessMemoryDump::GetOrCreateAllocatorDump(
			const std::string& absolute_name) {
			MemoryAllocatorDump* mad = GetAllocatorDump(absolute_name);
			return mad ? mad : CreateAllocatorDump(absolute_name);
		}

		MemoryAllocatorDump* ProcessMemoryDump::CreateSharedGlobalAllocatorDump(	
			const MemoryAllocatorDumpGuid& guid) {
			// A shared allocator dump can be shared within a process and the guid could
			// have been created already.
			MemoryAllocatorDump* mad = GetSharedGlobalAllocatorDump(guid);
			if (mad && mad != black_hole_mad_.get()) {
				// The weak flag is cleared because this method should create a non-weak
				// dump.
				mad->clear_flags(MemoryAllocatorDump::Flags::WEAK);
				return mad;
			}
			return CreateAllocatorDump(GetSharedGlobalAllocatorDumpName(guid), guid);
		}

		MemoryAllocatorDump* ProcessMemoryDump::CreateWeakSharedGlobalAllocatorDump(
			const MemoryAllocatorDumpGuid& guid) {
			MemoryAllocatorDump* mad = GetSharedGlobalAllocatorDump(guid);
			if (mad && mad != black_hole_mad_.get())
				return mad;
			mad = CreateAllocatorDump(GetSharedGlobalAllocatorDumpName(guid), guid);
			mad->set_flags(MemoryAllocatorDump::Flags::WEAK);
			return mad;
		}

		MemoryAllocatorDump* ProcessMemoryDump::GetSharedGlobalAllocatorDump(
			const MemoryAllocatorDumpGuid& guid) const {
			return GetAllocatorDump(GetSharedGlobalAllocatorDumpName(guid));
		}

		void ProcessMemoryDump::DumpHeapUsage(
			const std::unordered_map<base::trace_event::AllocationContext,
			base::trace_event::AllocationMetrics>&
			metrics_by_context,
			base::trace_event::TraceEventMemoryOverhead& overhead,
			const char* allocator_name) {
			const auto base_name = base::StringPrintf("tracing/heap_profiler_%s", 
													   allocator_name);
			overhead.DumpInto(base_name.c_str(), this);
		}

		void ProcessMemoryDump::SetAllocatorDumpsForSerialization(
			std::vector<std::unique_ptr<MemoryAllocatorDump>> dumps) {
			DCHECK(allocator_dumps_.empty());
			for (std::unique_ptr<MemoryAllocatorDump>& dump : dumps)
				AddAllocatorDumpInternal(std::move(dump));
		}

		std::vector<ProcessMemoryDump::MemoryAllocatorDumpEdge>
			ProcessMemoryDump::GetAllEdgesForSerialization() const {
			std::vector<MemoryAllocatorDumpEdge> edges;
			edges.reserve(allocator_dumps_edges_.size());
			for (const auto& it : allocator_dumps_edges_)
				edges.push_back(it.second);
			return edges;
		}

		void ProcessMemoryDump::SetAllEdgesForSerialization(
			const std::vector<ProcessMemoryDump::MemoryAllocatorDumpEdge>& edges) {
			DCHECK(allocator_dumps_edges_.empty());
			for (const MemoryAllocatorDumpEdge& edge : edges) {
				const auto it_and_inserted = allocator_dumps_edges_.emplace(edge.source, edge);
				DCHECK(it_and_inserted.second);
			}
		}

		void ProcessMemoryDump::Clear() {
			allocator_dumps_.clear();
			allocator_dumps_edges_.clear();
		}

		void ProcessMemoryDump::TakeAllDumpsFrom(ProcessMemoryDump* other) {
			// Moves the ownership of all MemoryAllocatorDump(s) contained in |other|
			// into this ProcessMemoryDump, checking for duplicates.
			for (auto& it : other->allocator_dumps_)
				AddAllocatorDumpInternal(std::move(it.second));
			other->allocator_dumps_.clear();

			// Move all the edges.
			allocator_dumps_edges_.insert(other->allocator_dumps_edges_.begin(),
				other->allocator_dumps_edges_.end());
			other->allocator_dumps_edges_.clear();
		}

		void ProcessMemoryDump::SerializeAllocatorDumpsInto(TracedValue* value) const {
			if (allocator_dumps_.size() > 0) {
				value->BeginDictionary("allocators");
				for (const auto& allocator_dump_it : allocator_dumps_)
					allocator_dump_it.second->AsValueInto(value);
				value->EndDictionary();
			}

			value->BeginArray("allocators_graph");
			for (const auto& it : allocator_dumps_edges_) {
				const MemoryAllocatorDumpEdge& edge = it.second;
				value->BeginDictionary();
				value->SetString("source", edge.source.ToString());
				value->SetString("target", edge.target.ToString());
				value->SetInteger("importance", edge.importance);
				value->SetString("type", kEdgeTypeOwnership);
				value->EndDictionary();
			}
			value->EndArray();
		}

		void ProcessMemoryDump::AddOwnershipEdge(const MemoryAllocatorDumpGuid& source,
			const MemoryAllocatorDumpGuid& target,
			int importance) {
			// This will either override an existing edge or create a new one.
			const auto it = allocator_dumps_edges_.find(source);
			auto max_importance = importance;
#undef max
			if (it != allocator_dumps_edges_.end()) {
				DCHECK_EQ(target.ToUint64(), it->second.target.ToUint64());
				max_importance = std::max(importance, it->second.importance);
			}
			allocator_dumps_edges_[source] = { source, target, max_importance,
				false /* overridable */ };
		}

		void ProcessMemoryDump::AddOwnershipEdge(
			const MemoryAllocatorDumpGuid& source,
			const MemoryAllocatorDumpGuid& target) {
			AddOwnershipEdge(source, target, 0 /* importance */);
		}

		void ProcessMemoryDump::AddOverridableOwnershipEdge(
			const MemoryAllocatorDumpGuid& source,
			const MemoryAllocatorDumpGuid& target,
			int importance) {
			if (allocator_dumps_edges_.count(source) == 0) {
				allocator_dumps_edges_[source] = { source, target, importance,
					true /* overridable */ };
			} else {
				// An edge between the source and target already exits. So, do nothing here
				// since the new overridable edge is implicitly overridden by a strong edge
				// which was created earlier.
				DCHECK(!allocator_dumps_edges_[source].overridable);
			}
		}

		void ProcessMemoryDump::CreateSharedMemoryOwnershipEdge(
			const MemoryAllocatorDumpGuid& client_local_dump_guid,
			const UnguessableToken& shared_memory_guid,
			int importance) {
			CreateSharedMemoryOwnershipEdgeInternal(client_local_dump_guid,
				shared_memory_guid, importance,
				false /*is_weak*/);
		}

		void ProcessMemoryDump::CreateWeakSharedMemoryOwnershipEdge(
			const MemoryAllocatorDumpGuid& client_local_dump_guid,
			const UnguessableToken& shared_memory_guid,
			int importance) {
			CreateSharedMemoryOwnershipEdgeInternal(
				client_local_dump_guid, shared_memory_guid, importance, true /*is_weak*/);
		}

		void ProcessMemoryDump::CreateSharedMemoryOwnershipEdgeInternal(
			const MemoryAllocatorDumpGuid& client_local_dump_guid,
			const UnguessableToken& shared_memory_guid,
			int importance,
			bool is_weak) {
			DCHECK(!shared_memory_guid.is_empty());
			// New model where the global dumps created by SharedMemoryTracker are used
			// for the clients.

			// The guid of the local dump created by SharedMemoryTracker for the memory
			// segment.
			const auto local_shm_guid = 
				GetDumpId(SharedMemoryTracker::GetDumpNameForTracing(shared_memory_guid));

			// The dump guid of the global dump created by the tracker for the memory
			// segment.
			const auto global_shm_guid = 
				SharedMemoryTracker::GetGlobalDumpIdForTracing(shared_memory_guid);

			// Create an edge between local dump of the client and the local dump of the
			// SharedMemoryTracker. Do not need to create the dumps here since the tracker
			// would create them. The importance is also required here for the case of
			// single process mode.
			AddOwnershipEdge(client_local_dump_guid, local_shm_guid, importance);

			// TODO(ssid): Handle the case of weak dumps here. This needs a new function
			// GetOrCreaetGlobalDump() in PMD since we need to change the behavior of the
			// created global dump.
			// Create an edge that overrides the edge created by SharedMemoryTracker.
			AddOwnershipEdge(local_shm_guid, global_shm_guid, importance);
		}

		void ProcessMemoryDump::AddSuballocation(const MemoryAllocatorDumpGuid& source, 
												 const std::string& target_node_name) {
			// Do not create new dumps for suballocations in background mode.
			if (dump_args_.level_of_detail == MemoryDumpLevelOfDetail::BACKGROUND)
				return;

			const auto child_mad_name = target_node_name + "/__" + source.ToString();
			const auto target_child_mad = CreateAllocatorDump(child_mad_name);
			AddOwnershipEdge(source, target_child_mad->guid());
		}

		MemoryAllocatorDump* ProcessMemoryDump::GetBlackHoleMad() {
			DCHECK(is_black_hole_non_fatal_for_testing_);
			if (!black_hole_mad_) {
				const std::string name = "discarded";
				black_hole_mad_.reset(new MemoryAllocatorDump(
					name, dump_args_.level_of_detail, GetDumpId(name)));
			}
			return black_hole_mad_.get();
		}

		MemoryAllocatorDumpGuid ProcessMemoryDump::GetDumpId(
			const std::string& absolute_name) const {
			return MemoryAllocatorDumpGuid(StringPrintf(
				"%s:%s", process_token().ToString().c_str(), absolute_name.c_str()));
		}

		bool ProcessMemoryDump::MemoryAllocatorDumpEdge::operator==(
			const MemoryAllocatorDumpEdge& other) const {
			return source == other.source && target == other.target && 
				importance == other.importance && overridable == other.overridable;
		}

		bool ProcessMemoryDump::MemoryAllocatorDumpEdge::operator!=(
			const MemoryAllocatorDumpEdge& other) const {
			return !(*this == other);
		}

	}  // namespace trace_event
}  // namespace base
