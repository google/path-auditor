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

#include "pathauditor/process_information.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "pathauditor/util/path.h"
#include "pathauditor/util/statusor.h"
#include "absl/strings/str_cat.h"
#include "pathauditor/util/canonical_errors.h"
#include "pathauditor/util/status_macros.h"

namespace pathauditor {

namespace {

StatusOr<int> OpenFile(absl::string_view path, int open_flags) {
  int fd = open(std::string(path).c_str(), open_flags);
  if (fd == -1) {
    return FailedPreconditionError(
        absl::StrCat("Could not open \"", path, "\""));
  }
  return fd;
}

}  // namespace

RemoteProcessInformation::RemoteProcessInformation(
    pid_t pid, absl::string_view cwd, absl::optional<std::string> cmdline,
    bool fallback)
    : pid_(pid), cwd_(cwd), cmdline_(std::move(cmdline)), fallback_(fallback) {}

StatusOr<int> RemoteProcessInformation::OpenFileInProc(
    absl::string_view path, int open_flags) const {
  return OpenFile(JoinPath("/proc", absl::StrCat(pid_), path),
                  open_flags);
}

StatusOr<int> RemoteProcessInformation::DupDirFileDescriptor(
    int fd, int open_flags) const {
  return OpenFileInProc(JoinPath("fd", absl::StrCat(fd)), open_flags);
}

StatusOr<int> RemoteProcessInformation::CwdFileDescriptor(
    int open_flags) const {
  // The root of the target process might not be the same as ours. Try to
  // resolve it relative to /proc/<pid>/root
  StatusOr<int> maybe_fd =
      OpenFileInProc(JoinPath("root", cwd_), open_flags);
  if (maybe_fd.ok() || !fallback_) {
    return maybe_fd;
  }
  // Fallback if the process doesn't exist anymore.
  return OpenFile(cwd_, open_flags);
}

StatusOr<int> RemoteProcessInformation::RootFileDescriptor(
    int open_flags) const {
  StatusOr<int> maybe_fd = OpenFileInProc("root", open_flags);
  if (maybe_fd.ok() || !fallback_) {
    return maybe_fd;
  }
  // Fallback if the process doesn't exist anymore.
  return OpenFile("/", open_flags);
}

pid_t RemoteProcessInformation::Pid() const { return pid_; }

std::string RemoteProcessInformation::Cwd() const { return cwd_; }

std::string RemoteProcessInformation::Cmdline() const {
  return cmdline_.value_or("");
}

StatusOr<int> SameProcessInformation::DupDirFileDescriptor(
    int fd, int open_flags) const {
  // use openat instead of dup so that we control the flags
  int new_fd = openat(fd, ".", open_flags);
  if (new_fd == -1) {
    return FailedPreconditionError("openat on dir fd failed");
  }
  return new_fd;
}

StatusOr<int> SameProcessInformation::CwdFileDescriptor(
    int open_flags) const {
  return OpenFile(".", open_flags);
}

StatusOr<int> SameProcessInformation::RootFileDescriptor(
    int open_flags) const {
  return OpenFile("/", open_flags);
}

}  // namespace pathauditor
