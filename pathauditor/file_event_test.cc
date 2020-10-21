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

#include <sys/syscall.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "pathauditor/util/status_matchers.h"
#include "util/task/status.h"

namespace pathauditor {
namespace {

using ::testing::Eq;
using pathauditor::IsOkAndHolds;
using pathauditor::StatusIs;

TEST(FileEventTest, ReturnsSyscallNr) {
  FileEvent event(SYS_open, {0}, {"/foo"});
  EXPECT_THAT(event.syscall_nr, Eq(SYS_open));
}

TEST(FileEventTest, ArgumentAccess) {
  FileEvent event(SYS_open, {10, 20}, {"/foo", "/bar"});
  EXPECT_THAT(event.Arg(0), IsOkAndHolds(10));
  EXPECT_THAT(event.Arg(1), IsOkAndHolds(20));
  EXPECT_THAT(event.PathArg(0), IsOkAndHolds("/foo"));
  EXPECT_THAT(event.PathArg(1), IsOkAndHolds("/bar"));
}

TEST(FileEventTest, EmptyArguments) {
  FileEvent event(SYS_open, {}, {});
  EXPECT_THAT(event.Arg(0), StatusIs(absl::StatusCode::kOutOfRange));
  EXPECT_THAT(event.PathArg(0), StatusIs(absl::StatusCode::kOutOfRange));
}

TEST(FileEventTest, NegativeIndex) {
  FileEvent event(SYS_open, {0, 0}, {"/foo", "/bar"});
  EXPECT_THAT(event.Arg(-1), StatusIs(absl::StatusCode::kOutOfRange));
  EXPECT_THAT(event.PathArg(-1), StatusIs(absl::StatusCode::kOutOfRange));
}

}  // namespace
}  // namespace pathauditor
