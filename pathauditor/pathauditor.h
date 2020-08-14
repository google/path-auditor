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

#ifndef PATHAUDITOR_PATHAUDITOR_H_
#define PATHAUDITOR_PATHAUDITOR_H_

#include <sys/types.h>

#include "pathauditor/util/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "pathauditor/file_event.h"
#include "pathauditor/process_information.h"

namespace pathauditor {

// Checks if any element in the path could have been replaced with a symlink by
// an unprivileged user.
// If the path is relative, at_fd needs to be a valid file descriptor.
// The max_iteration_count is to guard against symlink loops.
StatusOr<bool> PathIsUserControlled(
    const ProcessInformation &proc_info, absl::string_view path,
    absl::optional<int> at_fd = absl::optional<int>(),
    unsigned int max_iteration_count = 40);

// Perform checks on the paths in the FileEvent based on the syscall and its
// arguments.
// For example, if open is called with the O_NOFOLLOW flag, we can skip the last
// element in the path.
StatusOr<bool> FileEventIsUserControlled(
    const ProcessInformation &proc_info, const FileEvent &event);

}  // namespace pathauditor

#endif  // PATHAUDITOR_PATHAUDITOR_H_
