# LD_PRELOAD based path auditor for libc.

licenses(["notice"])

cc_binary(
    name = "libpath_auditor.so",
    srcs = ["path_auditor_libc.cc"],
    linkopts = ["-ldl"],
    linkshared = True,
    visibility = ["//visibility:public"],
    deps = [
        ":logging",
        "//pathauditor/util:statusor",
        "//pathauditor",
        "//pathauditor:file_event",
        "//pathauditor:process_information",
        "//pathauditor/util:status",
    ],
)

cc_library(
    name = "logging",
    srcs = ["logging.cc"],
    hdrs = ["logging.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/debugging:stacktrace",
        "@com_google_absl//absl/debugging:symbolize",
        "@com_google_absl//absl/strings:str_format",
        "//pathauditor:file_event",
        "//pathauditor:process_information",
        "//pathauditor/util:status",
    ],
)
