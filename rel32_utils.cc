// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/rel32_utils.h"

#include <algorithm>

#include "base/check_op.h"
#include "components/zucchini/io_utils.h"

namespace zucchini {

/******** Rel32ReaderX86 ********/

Rel32ReaderX86::Rel32ReaderX86(ConstBufferView image,
                               offset_t lo,
                               offset_t hi,
                               const std::deque<offset_t>* locations,
                               const AddressTranslator& translator)
    : image_(image),
      target_rva_to_offset_(translator),
      location_offset_to_rva_(translator),
      hi_(hi),
      last_(locations->end()) {
  DCHECK_LE(lo, image.size());
  DCHECK_LE(hi, image.size());
  current_ = std::lower_bound(locations->begin(), locations->end(), lo);
}

Rel32ReaderX86::~Rel32ReaderX86() = default;

std::optional<Reference> Rel32ReaderX86::GetNext() {
  while (current_ < last_ && *current_ < hi_) {
    offset_t loc_offset = *(current_++);
    DCHECK_LE(loc_offset + 4, image_.size());  // Sanity check.
    rva_t loc_rva = location_offset_to_rva_.Convert(loc_offset);
    rva_t target_rva = loc_rva + 4 + image_.read<int32_t>(loc_offset);
    offset_t target_offset = target_rva_to_offset_.Convert(target_rva);
    // |locations| is valid by assumption (see class description).
    DCHECK_NE(kInvalidOffset, target_offset);
    return Reference{loc_offset, target_offset};
  }
  return std::nullopt;
}

/******** Rel32ReceptorX86 ********/

Rel32WriterX86::Rel32WriterX86(MutableBufferView image,
                               const AddressTranslator& translator)
    : image_(image),
      target_offset_to_rva_(translator),
      location_offset_to_rva_(translator) {}

Rel32WriterX86::~Rel32WriterX86() = default;

void Rel32WriterX86::PutNext(Reference ref) {
  rva_t target_rva = target_offset_to_rva_.Convert(ref.target);
  rva_t loc_rva = location_offset_to_rva_.Convert(ref.location);

  // Subtraction underflow is okay
  uint32_t code =
      static_cast<uint32_t>(target_rva) - (static_cast<uint32_t>(loc_rva) + 4);
  image_.write<uint32_t>(ref.location, code);
}

}  // namespace zucchini
