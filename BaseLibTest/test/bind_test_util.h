// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "bind.h"
#include <string>

namespace base {

	class Location;

	namespace internal {

		template <typename F, typename Signature>
		struct BindLambdaHelper;

		template <typename F, typename R, typename... Args>
		struct BindLambdaHelper<F, R(Args...)> {
			static R Run(const std::decay_t<F>& f, Args... args) {
				return f(std::forward<Args>(args)...);
			}
		};

	}  // namespace internal

	// A variant of Bind() that can bind capturing lambdas for testing.
	// This doesn't support extra arguments binding as the lambda itself can do.
	template <typename F>
	decltype(auto) BindLambdaForTesting(F&& f) {
		using Signature = internal::ExtractCallableRunType<std::decay_t<F>>;
		return BindRepeating(&internal::BindLambdaHelper<F, Signature>::Run,
			std::forward<F>(f));
	}

	// Returns a closure that fails on destruction if it hasn't been run.
	OnceClosure MakeExpectedRunClosure(const Location& location,
		std::string_view message = std::string_view());
	RepeatingClosure MakeExpectedRunAtLeastOnceClosure(
		const Location& location,
		std::string_view message = std::string_view());

	// Returns a closure that fails the test if run.
	RepeatingClosure MakeExpectedNotRunClosure(const Location& location,
		std::string_view message = std::string_view());

}  // namespace base
