#pragma once

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base_export.h"
#include "build_config.h"

namespace base {

	enum IntegrityLevel {
		INTEGRITY_UNKNOWN,
		UNTRUSTED_INTEGRITY,
		LOW_INTEGRITY,
		MEDIUM_INTEGRITY,
		HIGH_INTEGRITY,
	};

	// Returns the integrity level of the process. Returns INTEGRITY_UNKNOWN in the
	// case of an underlying system failure.
	BASE_EXPORT IntegrityLevel GetCurrentProcessIntegrityLevel();

	// Determines whether the current process is elevated.
	BASE_EXPORT bool IsCurrentProcessElevated();

}  // namespace base

