# The pathauditor is a project meant to detect insecure file access patterns
# performed by privileged processes.

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

exports_files(["LICENSE"])

cc_library(
    name = "pathauditor",
    srcs = ["pathauditor.cc"],
    hdrs = ["pathauditor.h"],
    deps = [
        ":file_event",
        ":process_information",
        "//pathauditor/util:cleanup",
        "//pathauditor/util:path",
        "//pathauditor/util:status_macros",
        "@com_google_absl//absl/container:fixed_array",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_glog//:glog",
    ],
)

cc_library(
    name = "process_information",
    srcs = ["process_information.cc"],
    hdrs = ["process_information.h"],
    deps = [
        "//pathauditor/util:path",
        "//pathauditor/util:status_macros",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "file_event",
    srcs = ["file_event.cc"],
    hdrs = ["file_event.h"],
    deps = [
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "file_event_test",
    srcs = ["file_event_test.cc"],
    deps = [
        ":file_event",
        "//pathauditor/util:status_matchers",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)
