// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win/vector.h"

namespace base::win::internal
{

			HRESULT VectorChangedEventArgs::get_CollectionChange(
				ABI::Windows::Foundation::Collections::CollectionChange* value) {
				*value = change_;
				return S_OK;
			}

			HRESULT VectorChangedEventArgs::get_Index(unsigned int* value) {
				*value = index_;
				return S_OK;
			}
} // namespace base