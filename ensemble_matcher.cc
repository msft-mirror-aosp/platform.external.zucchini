// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/ensemble_matcher.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "base/stl_util.h"

namespace zucchini {

/******** EnsembleMatcher ********/

EnsembleMatcher::EnsembleMatcher() = default;

EnsembleMatcher::~EnsembleMatcher() = default;

void EnsembleMatcher::Trim() {
  // Trim rule: If > 1 DEX files are found then ignore all DEX. This is done
  // because we do not yet support MultiDex, under which contents can move
  // across file boundary between "old" and "new" archives. When this occurs,
  // forcing matches of DEX files and patching them separately can result in
  // larger patches than naive patching.
  auto is_match_dex = [](const ElementMatch& match) {
    return match.exe_type() == kExeTypeDex;
  };
  auto num_dex = std::count_if(matches_.begin(), matches_.end(), is_match_dex);
  if (num_dex > 1) {
    LOG(WARNING) << "Found " << num_dex << " DEX: Ignoring all.";
    base::EraseIf(matches_, is_match_dex);
  }
}

}  // namespace zucchini
