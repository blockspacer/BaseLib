#pragma once

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base_export.h"
#include "metrics/field_trial_params.h"

namespace base {
	struct Feature;

	extern const BASE_EXPORT Feature kAllTasksUserBlocking;
	extern const BASE_EXPORT Feature kMergeBlockingNonBlockingPools;

	// Under this feature, unused threads in ThreadGroup are only detached
	// if the total number of threads in the pool is above the initial capacity.
	extern const BASE_EXPORT Feature kNoDetachBelowInitialCapacity;

	// Under this feature, workers blocked with MayBlock are replaced immediately
	// instead of waiting for a threshold.
	extern const BASE_EXPORT Feature kMayBlockWithoutDelay;

#define HAS_NATIVE_THREAD_POOL() 1

#if HAS_NATIVE_THREAD_POOL()
	// Under this feature, ThreadPoolImpl will use a ThreadGroup backed by a
	// native thread pool implementation. The Windows Thread Pool API and
	// libdispatch are used on Windows and macOS/iOS respectively.
	extern const BASE_EXPORT Feature kUseNativeThreadPool;
#endif

	// Whether threads in the ThreadPool should be reclaimed after being idle for 5
	// minutes, instead of 30 seconds.
	extern const BASE_EXPORT Feature kUseFiveMinutesThreadReclaimTime;

}  // namespace base
