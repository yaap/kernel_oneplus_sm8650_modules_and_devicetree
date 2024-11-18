load("//build/kernel/kleaf:kernel.bzl", "ddk_headers")
load("//build/kernel/oplus:oplus_modules_define.bzl", "define_oplus_ddk_module")
load("//build/kernel/oplus:oplus_modules_dist.bzl", "ddk_copy_to_dist_dir")

def define_oplus_local_modules():

    define_oplus_ddk_module(
        name = "oplus_bsp_tp_syna_common",
        srcs = native.glob([
            "**/*.h",
            "Synaptics/synaptics_touch_panel_remote.c",
            "Synaptics/synaptics_common.c",
        ]),
        includes = ["."],
        ko_deps = [
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_custom",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_common",
        ],
#        local_defines = ["CONFIG_REMOVE_OPLUS_FUNCTION"],
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_tp_tcm_S3910",
        srcs = native.glob([
            "**/*.h",
            "Synaptics/Syna_tcm_S3910/synaptics_tcm_S3910.c",
            "Synaptics/Syna_tcm_S3910/synaptics_tcm_device_S3910.c",
        ]),
        ko_deps = [
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_custom",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_common",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_syna_common",
        ],
        includes = ["."],
#        local_defines = ["CONFIG_REMOVE_OPLUS_FUNCTION"],
	conditional_defines = {
            "mtk":  ["CONFIG_TOUCHPANEL_MTK_PLATFORM","CONFIG_TOUCHPANEL_MULTI_NOFLASH"],
        },
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_tp_novatek_common",
        srcs = native.glob([
            "**/*.h",
            "Novatek/novatek_common.c",
        ]),
        includes = ["."],
        ko_deps = [
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_custom",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_common",
        ],
#        local_defines = ["CONFIG_REMOVE_OPLUS_FUNCTION"],
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_tp_nt36672c_noflash",
        srcs = native.glob([
            "**/*.h",
            "Novatek/NT36672C_noflash/nvt_drivers_nt36672c_noflash.c",
        ]),
        ko_deps = [
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_custom",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_common",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_novatek_common",
        ],
        includes = ["."],
#        local_defines = ["CONFIG_REMOVE_OPLUS_FUNCTION"],
        conditional_defines = {
            "mtk":  ["CONFIG_TOUCHPANEL_MTK_PLATFORM","CONFIG_TOUCHPANEL_MULTI_NOFLASH"],
        },
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_tp_nt36532_noflash",
        srcs = native.glob([
            "**/*.h",
            "Novatek/NT36532_noflash/nvt_drivers_nt36532_noflash.c",
        ]),
        ko_deps = [
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_custom",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_common",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_novatek_common",
        ],
        includes = ["."],
#        local_defines = ["CONFIG_REMOVE_OPLUS_FUNCTION"],
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_tp_ilitek_common",
        srcs = native.glob([
            "**/*.h",
            "ilitek/ilitek_common.c",
        ]),
        includes = ["."],
        ko_deps = [
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_custom",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_common",
        ],
#        local_defines = ["CONFIG_REMOVE_OPLUS_FUNCTION"],
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_tp_ilitek7807s",
        srcs = native.glob([
            "**/*.h",
            "ilitek/ilitek7807s/ili7807s_fw.c",
            "ilitek/ilitek7807s/ili7807s_ic.c",
            "ilitek/ilitek7807s/ili7807s_mp.c",
            "ilitek/ilitek7807s/ili7807s_node.c",
            "ilitek/ilitek7807s/ili7807s_qcom.c",
        ]),
        ko_deps = [
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_custom",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_common",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_ilitek_common",
        ],
        includes = ["."],
#        local_defines = ["CONFIG_REMOVE_OPLUS_FUNCTION"],
        conditional_defines = {
            "mtk":  ["CONFIG_TOUCHPANEL_MTK_PLATFORM","CONFIG_TOUCHPANEL_MULTI_NOFLASH"],
        },
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_tp_ft3683g",
        srcs = native.glob([
            "**/*.h",
            "Focal/ft3683g/ft3683g_driver.c",
            "Focal/ft3683g/ft3683g_test.c",
        ]),
        ko_deps = [
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_custom",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_common",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_focal_common",
        ],
        includes = ["."],
#        local_defines = ["CONFIG_REMOVE_OPLUS_FUNCTION"],
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_tp_focal_common",
        srcs = native.glob([
            "**/*.h",
            "Focal/focal_common.c",
        ]),
        includes = ["."],
        ko_deps = [
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_custom",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_common",
        ],
#        local_defines = ["CONFIG_REMOVE_OPLUS_FUNCTION"],
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_tp_ft3681",
        srcs = native.glob([
            "**/*.h",
            "Focal/ft3681/*.c",
        ]),
        ko_deps = [
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_custom",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_common",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_focal_common",
        ],
        includes = ["."],
#        local_defines = ["CONFIG_REMOVE_OPLUS_FUNCTION"],
        conditional_defines = {
            "mtk":  ["CONFIG_TOUCHPANEL_MTK_PLATFORM"],
        },
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_tp_ft3658u_spi",
        srcs = native.glob([
            "**/*.h",
            "Focal/ft3658u_spi/ft3658u_driver.c",
            "Focal/ft3658u_spi/ft3658u_test.c",
        ]),
        ko_deps = [
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_custom",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_common",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_focal_common",
        ],
        includes = ["."],
#        local_defines = ["CONFIG_REMOVE_OPLUS_FUNCTION"],
        conditional_defines = {
            "mtk":  ["CONFIG_TOUCHPANEL_MTK_PLATFORM"],
        },
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_tp_ft3518",
        srcs = native.glob([
            "**/*.h",
            "Focal/ft3518/ft3518_driver.c",
            "Focal/ft3518/ft3518_test.c",
        ]),
        ko_deps = [
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_custom",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_common",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_focal_common",
        ],
        includes = ["."],
#        local_defines = ["CONFIG_REMOVE_OPLUS_FUNCTION"],
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_tp_goodix_comnon",
        srcs = native.glob([
            "**/*.h",
            "Goodix/gtx8_tools.c",
            "Goodix/goodix_common.c",
        ]),
        ko_deps = [
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_custom",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_common",
        ],
        includes = ["."],
#        local_defines = ["CONFIG_REMOVE_OPLUS_FUNCTION"],
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_tp_gt9966",
        srcs = native.glob([
            "**/*.h",
            "Goodix/GT9966/goodix_brl_core.c",
            "Goodix/GT9966/goodix_pen.c",
        ]),
        ko_deps = [
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_custom",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_common",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_goodix_comnon",
        ],
        includes = ["."],
#        local_defines = ["CONFIG_REMOVE_OPLUS_FUNCTION"],
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_tp_gt9916",
        srcs = native.glob([
            "**/*.h",
            "Goodix/GT9916/goodix_brl_core.c",
            "Goodix/GT9916/goodix_pen.c",
        ]),
        ko_deps = [
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_custom",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_common",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_goodix_comnon",
        ],
        includes = ["."],
#        local_defines = ["CONFIG_REMOVE_OPLUS_FUNCTION"],
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_tp_td4377_noflash",
        srcs = native.glob([
            "**/*.h",
            "Synaptics/TD4377_noflash/synaptics_tcm_core.c",
            "Synaptics/TD4377_noflash/synaptics_tcm_device.c",
            "Synaptics/TD4377_noflash/synaptics_tcm_recovery.c",
            "Synaptics/TD4377_noflash/synaptics_tcm_zeroflash.c",
        ]),
        ko_deps = [
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_custom",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_common",
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_syna_common",
        ],
        includes = ["."],
#        local_defines = ["CONFIG_REMOVE_OPLUS_FUNCTION"],
        conditional_defines = {
            "mtk":  ["CONFIG_TOUCHPANEL_MTK_PLATFORM","CONFIG_TOUCHPANEL_MULTI_NOFLASH"],
        },
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_tp_custom",
        srcs = native.glob([
            "**/*.h",
            "touch_custom/touch.c"
        ]),
        includes = ["."],
        local_defines = ["CONFIG_OPLUS_FEATURE_OPROJECT"],
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_tp_common",
        srcs = native.glob([
            "**/*.h",
            "util_interface/touch_interfaces.c",
            "touch_comon_api/touch_comon_api.c",
            "touchpanel_healthinfo/touchpanel_healthinfo.c",
            "touchpanel_healthinfo/touchpanel_exception.c",
            "touchpanel_autotest/touchpanel_autotest.c",
            "touchpanel_prevention/touchpanel_prevention.c",
            "touchpanel_common_driver.c",
            "touchpanel_proc.c",
            "tp_ioctl.c",
            "touchpanel_tui_support/touchpanel_tui_support.c",
            "message_list.c",
            "touch_pen/touch_pen_core.c",
            "touch_pen/touch_pen_algo.c",
        ]),
        ko_deps = [
            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_custom",
#            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_fw_update",	#built in-tree
#            "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_notify",	#built in-tree
#            "//vendor/oplus/kernel/device_info/device_info:oplus_bsp_device_info",		#built in-tree
        ],
        includes = ["."],
        local_defines = ["CONFIG_TOUCHPANEL_NOTIFY", "CONFIG_TOUCHPANEL_OPLUS_MODULE"],
        conditional_defines = {
            "qcom":  ["CONFIG_QCOM_PANEL_EVENT_NOTIFIER"],
            "mtk":  ["CONFIG_OPLUS_MTK_DRM_GKI_NOTIFY", "CONFIG_TOUCHPANEL_MTK_PLATFORM"],
        },
    )

    ddk_headers(
        name = "config_headers",
        hdrs  = native.glob([
            "**/*.h",
        ]),
        includes = [".","touchpanel_notify"],
    )

    ddk_copy_to_dist_dir(
        name = "oplus_bsp_tp",
        module_list = [
            "oplus_bsp_tp_ft3683g",
            "oplus_bsp_tp_syna_common",
            "oplus_bsp_tp_tcm_S3910",
            "oplus_bsp_tp_td4377_noflash",
            "oplus_bsp_tp_novatek_common",
            "oplus_bsp_tp_nt36672c_noflash",
            "oplus_bsp_tp_nt36532_noflash",
            "oplus_bsp_tp_focal_common",
            "oplus_bsp_tp_ft3681",
            "oplus_bsp_tp_ft3658u_spi",
            "oplus_bsp_tp_ft3518",
            "oplus_bsp_tp_goodix_comnon",
            "oplus_bsp_tp_gt9966",
	    "oplus_bsp_tp_gt9916",
	    "oplus_bsp_tp_ilitek_common",
	    "oplus_bsp_tp_ilitek7807s",
	    "oplus_bsp_tp_custom",
            "oplus_bsp_tp_common",
        ],
    )
