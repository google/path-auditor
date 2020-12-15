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

#include "pathauditor/libc/logging.h"

#include <fcntl.h>
#include <syslog.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "absl/debugging/stacktrace.h"
#include "absl/debugging/symbolize.h"
#include "absl/strings/str_format.h"

namespace {

static const size_t kCmdlineMax = 1024;
static const size_t kMaxStackFrames = 20;
static const size_t kMaxSymbolLen = 64;

static const std::string &GetCmdline() {
  static std::string cmdline;

  if (!cmdline.empty()) {
    return cmdline;
  }

  int fd = syscall(SYS_open, "/proc/self/cmdline", O_RDONLY);
  if (fd == -1) {
    cmdline = "(unknown)";
    return cmdline;
  }
  char cmdline_buf[kCmdlineMax];
  ssize_t bytes = read(fd, cmdline_buf, sizeof(cmdline_buf) - 1);
  close(fd);
  if (bytes == -1) {
    cmdline = "(unknown)";
    return cmdline;
  }
  cmdline_buf[bytes] = 0;

  // /proc/self/cmdline separates arguments with null bytes. Turn them into
  // spaces.
  for (ssize_t i = 0; i < bytes - 1; i++) {
    if (cmdline_buf[i] == '\x00') {
      cmdline_buf[i] = ' ';
    }
  }
  cmdline = cmdline_buf;

  return cmdline;
}

static int GetUid() { return syscall(SYS_getuid); }

static std::string CurrentStackTrace() {
  void *stack_trace[kMaxStackFrames];
  int frame_cnt = absl::GetStackTrace(stack_trace, kMaxStackFrames, 2);

  std::vector<std::string> stack_trace_lines;
  for (int i = 0; i < frame_cnt; i++) {
    char tmp[kMaxSymbolLen] = "";
    const char *symbol = "(unknown)";
    if (absl::Symbolize(stack_trace[i], tmp, sizeof(tmp))) {
      symbol = tmp;
    }
    stack_trace_lines.push_back(absl::StrCat(
        "  ", absl::Hex(stack_trace[i], absl::kZeroPad12), " ", symbol));
  }

  return absl::StrJoin(stack_trace_lines, "\n");
}

}  // namespace

namespace pathauditor {

void LogInsecureAccess(const FileEvent &event, const char *function_name) {
  // for testing that functions get audited
  const char *env_p = std::getenv("PATHAUDITOR_TEST");
  if (env_p) {
    std::cerr << "AUDITING:" << function_name << std::endl;
    return;
  }

  openlog("pathauditor", LOG_PID, 0);

  std::string args = absl::StrJoin(event.args, ", ");
  std::string path_args = absl::StrJoin(event.path_args, ", ");
  std::string event_info = absl::StrFormat(
      "function %s, cmdline %s, syscall_nr %d, args %s, path args %s, uid %d, "
      "stack trace:\n%s",
      function_name, GetCmdline(), event.syscall_nr, args, path_args, GetUid(),
      CurrentStackTrace());

  syslog(LOG_WARNING, "InsecureAccess: %s", event_info.c_str());
}

void LogError(const absl::Status &status) {
  openlog("pathauditor", LOG_PID, 0);
  syslog(LOG_WARNING, "Cannot audit: %s",
         std::string(status.message()).c_str());
}

}  // namespace pathauditor
