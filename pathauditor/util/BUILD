# Copyright 2019 Google LLC. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

package(default_visibility = ["//pathauditor:__subpackages__"])

licenses(["notice"])  # Apache 2.0

# A custom fork of util/task/status.h. This will become obsolete and will be
# replaced once Abseil releases absl::Status.
cc_library(
    name = "status",
    srcs = [
        "canonical_errors.cc",
        "status.cc",
    ],
    hdrs = [
        "canonical_errors.h",
        "status.h",
        "status_internal.h",
        "status_macros.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "statusor",
    hdrs = ["statusor.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":raw_logging",
        ":status",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:log_severity",
        "@com_google_absl//absl/types:variant",
    ],
)

# gMock matchers for pathauditor::Status and pathauditor::StatusOr<T> and a gUnit printer
# extension for pathauditor::StatusOr<T>.
cc_library(
    name = "status_matchers",
    testonly = 1,
    hdrs = ["status_matchers.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":status",
        ":statusor",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest",
    ],
)

# Tests for the Status utility.
cc_test(
    name = "status_test",
    srcs = ["status_test.cc"],
    deps = [
        ":status",
        ":status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

# Tests for the StatusOr template class.
cc_test(
    name = "statusor_test",
    srcs = ["statusor_test.cc"],
    deps = [
        ":status",
        ":status_matchers",
        ":statusor",
        "@com_google_googletest//:gtest_main",
    ],
)

# Tests for the Status macros.
cc_test(
    name = "status_macros_test",
    srcs = ["status_macros_test.cc"],
    deps = [
        ":status",
        ":status_matchers",
        ":statusor",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

# Compatibility layer for Abseil's flags vs. gFlags
cc_library(
    name = "flags",
    hdrs = ["flag.h"],
)

# A library for cleanup objects.
cc_library(
    name = "cleanup",
    hdrs = ["cleanup.h"],
)

cc_library(
    name = "path",
    srcs = ["path.cc"],
    hdrs = ["path.h"],
    deps = [
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "raw_logging",
    srcs = ["raw_logging.cc"],
    hdrs = ["raw_logging.h"],
    deps = [
        ":strerror",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "strerror",
    srcs = ["strerror.cc"],
    hdrs = ["strerror.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
    ],
)
