// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

// This header is a forwarding header to coalesce the various platform specific
// types representing MessagePumpForIO.

#include "build_config.h"

#if defined(OS_WIN)
#include "message_loop/message_pump_win.h"
#elif defined(OS_IOS)
#include "message_loop/message_pump_io_ios.h"
#elif defined(OS_MACOSX)
#include "message_loop/message_pump_kqueue.h"
#elif defined(OS_NACL_SFI)
#include "message_loop/message_pump_default.h"
#elif defined(OS_FUCHSIA)
#include "message_loop/message_pump_fuchsia.h"
#elif defined(OS_POSIX)
#include "message_loop/message_pump_libevent.h"
#endif

namespace base
{

#if defined(OS_WIN)
	// Windows defines it as-is.
	using MessagePumpForIO = MessagePumpForIO;
#elif defined(OS_IOS)
	using MessagePumpForIO = MessagePumpIOSForIO;
#elif defined(OS_MACOSX)
	using MessagePumpForIO = MessagePumpKqueue;
#elif defined(OS_NACL_SFI)
	using MessagePumpForIO = MessagePumpDefault;
#elif defined(OS_FUCHSIA)
	using MessagePumpForIO = MessagePumpFuchsia;
#elif defined(OS_POSIX)
	using MessagePumpForIO = MessagePumpLibevent;
#else
#error Platform does not define MessagePumpForIO
#endif

}  // namespace base
