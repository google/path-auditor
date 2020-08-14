// Copyright 2019 Google LLC. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "pathauditor/file_event.h"

#include "pathauditor/util/statusor.h"
#include "absl/strings/string_view.h"
#include "pathauditor/util/canonical_errors.h"

namespace pathauditor {

StatusOr<uint64_t> FileEvent::Arg(size_t idx) const {
  if (idx >= args.size()) {
    return OutOfRangeError(
        absl::StrCat("Index ", idx, " out of range (size ", args.size(), ")."));
  }
  return args[idx];
}

StatusOr<std::string> FileEvent::PathArg(size_t idx) const {
  if (idx >= path_args.size()) {
    return OutOfRangeError(absl::StrCat(
        "Index ", idx, " out of range (size ", path_args.size(), ")."));
  }
  return path_args[idx];
}

}  // namespace pathauditor
