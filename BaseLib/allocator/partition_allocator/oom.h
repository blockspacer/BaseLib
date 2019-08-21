#pragma once

// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "allocator/partition_allocator/oom_callback.h"
#include "logging.h"

#include <windows.h>

// Do not want trivial entry points just calling OOM_CRASH() to be
// commoned up by linker icf/comdat folding.
#define OOM_CRASH_PREVENT_ICF()                  \
  volatile int oom_crash_inhibit_icf = __LINE__; \
  ALLOW_UNUSED_LOCAL(oom_crash_inhibit_icf)

// OOM_CRASH() - Specialization of IMMEDIATE_CRASH which will raise a custom
// exception on Windows to signal this is OOM and not a normal assert.
#define OOM_CRASH()                                                     \
  do {                                                                  \
    OOM_CRASH_PREVENT_ICF();                                            \
    base::internal::RunPartitionAllocOomCallback();                     \
    ::RaiseException(0xE0000008, EXCEPTION_NONCONTINUABLE, 0, nullptr); \
    IMMEDIATE_CRASH();                                                  \
  } while (0)