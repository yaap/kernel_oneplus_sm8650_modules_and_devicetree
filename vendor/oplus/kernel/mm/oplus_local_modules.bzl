load("//build/kernel/kleaf:kernel.bzl", "ddk_headers")
load("//build/kernel/oplus:oplus_modules_define.bzl", "define_oplus_ddk_module")
load("//build/kernel/oplus:oplus_modules_dist.bzl", "ddk_copy_to_dist_dir")

def define_oplus_local_modules():

    define_oplus_ddk_module(
        name = "oplus_bsp_memleak_detect_simple",
        srcs = native.glob([
            "**/*.h",
            "memleak_detect/slub_track_simple.c",
            "memleak_detect/vmalloc_track_simple.c",
        ]),
        includes = ["."],
        copts = select({
            "//build/kernel/kleaf:kocov_is_true": ["-fprofile-arcs", "-ftest-coverage"],
            "//conditions:default": [],
        }),
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_zsmalloc",
        conditional_srcs = {
            "CONFIG_CONT_PTE_HUGEPAGE": {
                True: ["thp_zsmalloc/thp_zsmalloc.c"],
            }
        },

        srcs = native.glob([
            "**/*.h",
            "thp_zsmalloc/zsmalloc.c",
        ]),
        includes = ["."],
        copts = select({
            "//build/kernel/kleaf:kocov_is_true": ["-fprofile-arcs", "-ftest-coverage"],
            "//conditions:default": [],
        }),
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_sigkill_diagnosis",
        srcs = native.glob([
            "sigkill_diagnosis/sigkill_diagnosis.c",
        ]),
        includes = ["."],
        copts = select({
            "//build/kernel/kleaf:kocov_is_true": ["-fprofile-arcs", "-ftest-coverage"],
            "//conditions:default": [],
        }),
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_lz4k",
        srcs = native.glob([
            "**/*.h",
            "hybridswap_zram/lz4k/lz4k.c",
            "hybridswap_zram/lz4k/lz4k_compress.c",
            "hybridswap_zram/lz4k/lz4k_decompress.c",
        ]),
        includes = ["."],
        local_defines = ["CONFIG_CRYPTO_LZ4K"],
        copts = select({
            "//build/kernel/kleaf:kocov_is_true": ["-fprofile-arcs", "-ftest-coverage"],
            "//conditions:default": [],
        }),
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_dynamic_readahead",
        srcs = native.glob([
            "**/*.h",
            "dynamic_readahead/dynamic_readahead.c",
        ]),
        includes = ["."],
        local_defines = ["CONFIG_OPLUS_FEATURE_DYNAMIC_READAHEAD"],
        copts = select({
            "//build/kernel/kleaf:kocov_is_true": ["-fprofile-arcs", "-ftest-coverage"],
            "//conditions:default": [],
        }),
    )

    ddk_copy_to_dist_dir(
        name = "oplus_bsp_mm",
        module_list = [
            "oplus_bsp_memleak_detect_simple",
            "oplus_bsp_sigkill_diagnosis",
            "oplus_bsp_zsmalloc",
            "oplus_bsp_lz4k",
            # TODO: qcom convert to GKI implementation
            #"oplus_bsp_uxmem_opt",
            #"oplus_bsp_dynamic_readahead",
        ],
    )

