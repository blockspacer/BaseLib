// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "process/kill.h"

#include "bind.h"
#include "process/process_iterator.h"
#include "task/post_task.h"
#include "time/time.h"

namespace base {

	bool KillProcesses(const FilePath::StringType& executable_name, 
					   int exit_code, 
					   const ProcessFilter* filter) {
		auto result = true;
		NamedProcessIterator iter(executable_name, filter);
		while (const auto entry = iter.NextProcessEntry()) {
			auto process = Process::Open(entry->pid());
			// Sometimes process open fails. This would cause a DCHECK in
			// process.Terminate(). Maybe the process has killed itself between the
			// time the process list was enumerated and the time we try to open the
			// process?
			if (!process.IsValid()) {
				result = false;
				continue;
			}
			result &= process.Terminate(exit_code, true);
		}
		return result;
	}

	// Common implementation for platforms under which |process| is a handle to
	// the process, rather than an identifier that must be "reaped".
	void EnsureProcessTerminated(Process process) {
		DCHECK(!process.is_current());

		if (process.WaitForExitWithTimeout(TimeDelta(), nullptr))
			return;

		PostDelayedTask(
			FROM_HERE,
			{ThreadPool(), TaskPriority::BEST_EFFORT,
			 TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
			BindOnce(
			[](Process process) {
				if (process.WaitForExitWithTimeout(TimeDelta(), nullptr))
					return;
				process.Terminate(win::kProcessKilledExitCode, false);
			},
          	std::move(process)),
		TimeDelta::FromSeconds(2));
	}

}  // namespace base
