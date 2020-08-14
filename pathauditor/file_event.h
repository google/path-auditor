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

#ifndef PATHAUDITOR_FILE_EVENT_H_
#define PATHAUDITOR_FILE_EVENT_H_

#include <cstdint>
#include <string>
#include <vector>

#include "pathauditor/util/statusor.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"

namespace pathauditor {

// The FileEvent represents a fs-related syscall (open/rename/...).
struct FileEvent {
  FileEvent(int syscall_nr, const std::vector<uint64_t>& args,
            const std::vector<std::string>& path_args)
      : syscall_nr(syscall_nr), args(args), path_args(path_args) {}

  int syscall_nr;
  std::vector<uint64_t> args;
  std::vector<std::string> path_args;

  StatusOr<uint64_t> Arg(size_t idx) const;
  StatusOr<std::string> PathArg(size_t idx) const;
};

inline std::ostream& operator<<(std::ostream& os, FileEvent e) {
  return os << "syscall_nr: " << e.syscall_nr << ", args: ["
            << absl::StrJoin(e.args, ", ") << "], "
            << "path_args: [" << absl::StrJoin(e.path_args, ", ") << "]";
}

}  // namespace pathauditor

#endif  // PATHAUDITOR_FILE_EVENT_H_
