load("//build/kernel/kleaf:kernel.bzl", "ddk_headers")
load("//build/kernel/oplus:oplus_modules_define.bzl", "define_oplus_ddk_module")
load("//build/kernel/oplus:oplus_modules_dist.bzl", "ddk_copy_to_dist_dir")

def define_oplus_local_modules():

    define_oplus_ddk_module(
        name = "oplus_bsp_fpga_monitor",
        srcs = native.glob([
            "**/*.h",
            "fpga_monitor/fpga_exception.c",
            "fpga_monitor/fpga_healthinfo.c",
            "fpga_monitor/fpga_monitor.c",
        ]),
        includes = ["."],
    )
    define_oplus_ddk_module(
        name = "oplus_bsp_fpga_power",
        srcs = native.glob([
            "**/*.h",
            "fpga_power.c"
        ]),
        includes = ["."],
    )

    define_oplus_ddk_module(
        name = "oplus_fpga_upgrade",
        srcs = native.glob([
            "**/*.h",
            "gw_fpgadown.c"
        ]),
        includes = ["."],
    )
    ddk_copy_to_dist_dir(
        name = "oplus_bsp_fpga_monitor",
        module_list = [
             "oplus_bsp_fpga_monitor",
        ],
    )