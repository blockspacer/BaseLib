#pragma once

// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cerrno>

#include "base_export.h"
#include "macros.h"

namespace base {
	namespace internal {

		// ScopedClearLastError stores and resets the value of thread local error codes
		// (errno, GetLastError()), and restores them in the destructor. This is useful
		// to avoid side effects on these values in instrumentation functions that
		// interact with the OS.

		// Common implementation of ScopedClearLastError for all platforms. Use
		// ScopedClearLastError instead.
		class BASE_EXPORT ScopedClearLastErrorBase {
		public:
			ScopedClearLastErrorBase() : last_errno_(errno) { errno = 0; }
			~ScopedClearLastErrorBase() { errno = last_errno_; }

		private:
			const int last_errno_;

			DISALLOW_COPY_AND_ASSIGN(ScopedClearLastErrorBase);
		};

		// Windows specific implementation of ScopedClearLastError.
		class BASE_EXPORT ScopedClearLastError : public ScopedClearLastErrorBase {
		public:
			ScopedClearLastError();
			~ScopedClearLastError();

		private:
			unsigned int last_system_error_;

			DISALLOW_COPY_AND_ASSIGN(ScopedClearLastError);
		};

	}  // namespace internal
} // namespace base
