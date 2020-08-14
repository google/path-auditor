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

#ifndef PATHAUDITOR_PROCESS_INFORMATION_H_
#define PATHAUDITOR_PROCESS_INFORMATION_H_

#include <fcntl.h>

#include "pathauditor/util/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

namespace pathauditor {

// This interface is used by PathAuditor when checking fs-related
// syscalls. It will need to lookup file descriptors in the context of the
// process that made the syscall.
class ProcessInformation {
 public:
  virtual ~ProcessInformation() {}
  virtual StatusOr<int> DupDirFileDescriptor(int fd,
                                                   int open_flags) const = 0;
  virtual StatusOr<int> CwdFileDescriptor(int open_flags) const = 0;
  virtual StatusOr<int> RootFileDescriptor(int open_flags) const = 0;
};

// Represents the current process. CwdFileDescriptor will simply open(".") etc.
class SameProcessInformation : public ProcessInformation {
 public:
  StatusOr<int> DupDirFileDescriptor(int fd,
                                           int open_flags) const override;
  StatusOr<int> CwdFileDescriptor(int open_flags) const override;
  StatusOr<int> RootFileDescriptor(int open_flags) const override;
};

// Represents a remote process. File descriptors are looked up using the proc
// file system.
class RemoteProcessInformation : public ProcessInformation {
 public:
  // The pid and cwd will be used to lookup file descriptors.
  // cmdline is optional as it's only used for logging.
  // fallback controls what to do if the process doesn't exist anymore. If it's
  // set to true, it will fall back to the root of the current mount namespace
  // for file lookups.
  RemoteProcessInformation(
      pid_t pid, absl::string_view cwd,
      absl::optional<std::string> cmdline = absl::optional<std::string>(),
      bool fallback = false);

  StatusOr<int> DupDirFileDescriptor(int fd,
                                           int open_flags) const override;
  StatusOr<int> CwdFileDescriptor(int open_flags) const override;
  StatusOr<int> RootFileDescriptor(int open_flags) const override;

  pid_t Pid() const;
  std::string Cwd() const;
  std::string Cmdline() const;

 private:
  StatusOr<int> OpenFileInProc(absl::string_view path,
                                     int open_flags) const;
  pid_t pid_;
  std::string cwd_;
  absl::optional<std::string> cmdline_;
  bool fallback_;
};

}  // namespace pathauditor

#endif  // PATHAUDITOR_PROCESS_INFORMATION_H_
