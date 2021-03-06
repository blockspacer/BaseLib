// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base_export.h"
#include "callback_forward.h"
#include "containers/flat_set.h"
#include "location.h"
#include "task/common/checked_lock.h"
#include "task/task_traits.h"
#include "task_runner.h"
#include "time/time.h"

namespace base {
	namespace internal {

		class Sequence;
		class PooledTaskRunnerDelegate;

		// A task runner that runs tasks in parallel.
		class BASE_EXPORT PooledParallelTaskRunner : public TaskRunner {
		public:
			// Constructs a PooledParallelTaskRunner which can be used to post tasks.
			PooledParallelTaskRunner(
				const TaskTraits& traits,
				PooledTaskRunnerDelegate* pooled_task_runner_delegate);

			// TaskRunner:
			bool PostDelayedTask(const Location& from_here,
				OnceClosure closure,
				TimeDelta delay) override;

			bool RunsTasksInCurrentSequence() const override;

			// Removes |sequence| from |sequences_|.
			void UnregisterSequence(Sequence* sequence);

		private:
			~PooledParallelTaskRunner() override;

			const TaskTraits traits_;
			PooledTaskRunnerDelegate* const pooled_task_runner_delegate_;

			CheckedLock lock_;

			// List of alive Sequences instantiated by this PooledParallelTaskRunner.
			// Sequences are added when they are instantiated, and removed when they are
			// destroyed.
			flat_set<Sequence*> sequences_{};

			DISALLOW_COPY_AND_ASSIGN(PooledParallelTaskRunner);
		};

	}  // namespace internal
}  // namespace base
