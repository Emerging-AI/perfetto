load("@perfetto_cfg//:perfetto_cfg.bzl", "PERFETTO_CONFIG")

cc_library(
    name = "lib_gpu_counters",
    srcs = [
        "hwcpipe/include/hwcpipe/hwcpipe_counter.h",
        "backend/device/include/device/product_id.hpp"
    ],
    copts = [
    ] + PERFETTO_CONFIG.deps_copts.lib_gpu_counters,
    defines = [
    ],
    includes = [
        "hwcpipe/include",
        "backend/device/include",
    ],
    visibility = ["//visibility:public"],
)
