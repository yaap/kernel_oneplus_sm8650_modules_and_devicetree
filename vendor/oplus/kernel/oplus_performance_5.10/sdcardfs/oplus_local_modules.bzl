load("//build/kernel/kleaf:kernel.bzl", "ddk_headers")
load("//build/kernel/oplus:oplus_modules_define.bzl", "define_oplus_ddk_module")
load("//build/kernel/oplus:oplus_modules_dist.bzl", "ddk_copy_to_dist_dir")

def define_oplus_local_modules():

    define_oplus_ddk_module(
        name = "sdcardfs",
        srcs = native.glob([
            "*.h",
            "*.c",
        ]),
        local_defines = ["SDCARDFS_VERSION=\"0.1\""],
        includes = ["."],
        )

    ddk_copy_to_dist_dir(
        name = "sdcardfs",
        module_list = [
            "sdcardfs",
        ],
    )
