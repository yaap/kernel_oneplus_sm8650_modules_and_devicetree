/***
  magcvr means "magnetic cover"
**/

#include "../../magcvr_include/abstract/magnetic_cover.h"

// fault injection start
void fault_injection_check(struct magnetic_cover_info *magcvr_info, char *token)
{
	int i = 0;
	int max_cnt = sizeof(checkname) / sizeof(checkname[0]);

	for (i = 0; i < max_cnt; i++) {
		if (!strcmp(token, checkname[i].name))
			magcvr_info->fault_injection_opt |= 1 << checkname[i].opt;
	}

	if (!!(1 << OPT_CLEAR_ALL & magcvr_info->fault_injection_opt)) {
		magcvr_info->fault_injection_opt = 0;
		MAG_CVR_LOG("stop fault injection:%d\n", magcvr_info->fault_injection_opt);
	}
}

void other_opt_handle(struct magnetic_cover_info *magcvr_info, int opt)
{
	int debug_position = 0;

	if (!!(1 << OPT_DEBUG_ON & magcvr_info->fault_injection_opt)) {
		debug_enable = 1;
		MAG_CVR_LOG("enable debug log\n");
	} else if (!(1 << OPT_DEBUG_ON & magcvr_info->fault_injection_opt)){
		debug_enable = 0;
		MAG_CVR_LOG("disable debug log\n");
	}

	if (1 << OPT_NEAR_FAR & magcvr_info->fault_injection_opt) {
		switch (opt) {
			case MAGNETIC_COVER_INPUT_FAR:
				MAG_CVR_LOG("debug -->FAR\n");
				debug_position = MAGNETIC_COVER_INPUT_FAR;
			break;
			case MAGNETIC_COVER_INPUT_NEAR:
				MAG_CVR_LOG("debug -->NEAR\n");
				debug_position = MAGNETIC_COVER_INPUT_NEAR;
			break;
		}
#if IS_ENABLED(CONFIG_OPLUS_MAGCVR_NOTIFY)
		mag_call_notifier(debug_position);
#endif
	}
}

int fault_injection_handle(struct magnetic_cover_info *magcvr_info, int opt)
{
	MAG_CVR_DEBUG("fault_injection:%d\n", !!(1 << opt & magcvr_info->fault_injection_opt));
	return !!(1 << opt & magcvr_info->fault_injection_opt);
}
EXPORT_SYMBOL(fault_injection_handle);
// fault injection end

static int magnetic_cover_get_data(struct magnetic_cover_info *magcvr_info)
{
	int ret = 0;
	long value = 0;
	int get_data_retry = 0;

	if (magcvr_info == NULL) {
		MAG_CVR_ERR("g_magcvr_info NULL \n");
		return -EINVAL;
	}

	if (magcvr_info->mc_ops == NULL) {
		MAG_CVR_ERR("mc_ops NULL \n");
		return -EINVAL;
	}

	disable_irq(magcvr_info->irq);

	if (magcvr_info->mc_ops->get_data && magcvr_info->chip_info) {
		do {
			get_data_retry++;
			msleep(GET_DATA_TIMN);
			ret = magcvr_info->mc_ops->get_data(magcvr_info->chip_info, &value);
			MAG_CVR_DEBUG("get %d cnt data[%ld]\n", get_data_retry, value);
		} while (get_data_retry >= GET_DATA_RETRY);
		get_data_retry = 0;
		if (ret < 0 || fault_injection_handle(magcvr_info, OPT_IIC_READ)) {
			MAG_CVR_DEBUG("failed to get data\n");
			magcvr_info->iic_read_fail = 1;
			enable_irq(magcvr_info->irq);
			return 0;
		}
	} else {
		MAG_CVR_ERR("get_data api not found\n");
	}

	magcvr_info->m_value = m_long2int(value);
	MAG_CVR_LOG("value get[%d] and enable irq\n", magcvr_info->m_value);

	enable_irq(magcvr_info->irq);
	return magcvr_info->m_value;
}

// proc control for :: proc_magcvr_offset_read
static int proc_magcvr_farmax_th_read(struct seq_file *s, void *v)
{
	struct magnetic_cover_info *magcvr_info = s->private;

	int ret = 0;
	MAG_CVR_LOG("call");

	if (!magcvr_info) {
		MAG_CVR_ERR("g_magcvr_info null\n");
		seq_printf(s, "-1\n");
		return ret;
	} else {
		MAG_CVR_LOG("%d,%d,%d,",
		    magcvr_info->far_noise_th,
		    magcvr_info->far_threshold,
		    magcvr_info->far_max_th);
		seq_printf(s, "%d,%d,%d,",
		    magcvr_info->far_noise_th,
		    magcvr_info->far_threshold,
		    magcvr_info->far_max_th);
	}

	return ret;
}

// proc control for :: proc_magcvr_offset_read
static int proc_magcvr_config_read(struct seq_file *s, void *v)
{
	struct magnetic_cover_info *magcvr_info = s->private;

	int ret = 0;
	MAG_CVR_LOG("call");

	if (!magcvr_info) {
		MAG_CVR_ERR("chip_info null\n");
		seq_printf(s, "chip_info null\n");
		return ret;
	}

	if (!magcvr_info->mc_ops) {
		MAG_CVR_ERR("mc_ops null\n");
		seq_printf(s, "mc_ops null\n");
		return ret;
	}

	MAG_CVR_LOG( \
		"CHECK,offset:%d,step:%d,far_th:%d,far_noise_th:%d,position:%d,err_state:%d\n",
		magcvr_info->detect_offset,
		magcvr_info->detect_step,
		magcvr_info->far_threshold,
		magcvr_info->far_noise_th,
		magcvr_info->position,
		magcvr_info->err_state);

	seq_printf(s,
		"CHECK,offset:%d,step:%d,far_th:%d,far_noise_th:%d,position:%d,err_state:%d\n",
		magcvr_info->detect_offset,
		magcvr_info->detect_step,
		magcvr_info->far_threshold,
		magcvr_info->far_noise_th,
		magcvr_info->position,
		magcvr_info->err_state);

	if (magcvr_info->mc_ops->dump_regsiter) {
		MAG_CVR_DEBUG("dump_regsiter ops exist\n");
		magcvr_info->mc_ops->dump_regsiter(s, magcvr_info->chip_info);
	} else {
		MAG_CVR_ERR("not dump_regsiter ops\n");
	}

	return ret;
}

// proc control for :: proc_distance_calib_write
static ssize_t proc_distance_calib_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *ppos)
{
	struct magnetic_cover_info *magcvr_info = PDE_DATA(file_inode(file));

	int data = 0;
	char temp[128] = {0};
	int ret = -1;
	int i = 0;

	MAG_CVR_LOG("call.\n");
	if (!magcvr_info) {
		MAG_CVR_ERR("g_magcvr_info null\n");
		return count;
	}

	ret = copy_from_user(temp, buffer, count);
	if (ret) {
		 MAG_CVR_ERR("read proc input error.\n");
		return count;
	}
	MAG_CVR_DEBUG("Cali temp:[%s]\n", temp);

	if (sscanf(temp, "%d", &data)) {
		magcvr_info->detect_offset = data;
		// healthinfo for calibration data recoder
		if (magcvr_info->cal_offset_cnt >= CAL_OFFSET_MAX_CNT) {
			magcvr_info->cal_offset_cnt = 0;
			MAG_CVR_DEBUG("reset cal_offset_cnt->0\n");
		}
		magcvr_info->cal_offset[magcvr_info->cal_offset_cnt] = magcvr_info->detect_offset;
		magcvr_info->cal_offset_cnt++;

		for (i = 0; i < CAL_OFFSET_MAX_CNT; i++)
			MAG_CVR_LOG("healthinfo->offset[%d]:%d\n", i, magcvr_info->cal_offset[i]);

		magcvr_info->far_threshold = magcvr_info->ori_far_threshold + magcvr_info->detect_offset;
		magcvr_info->negative_far_threshold = magcvr_info->negative_ori_far_threshold - magcvr_info->detect_offset;
		magcvr_info->far_noise_th = magcvr_info->ori_far_noise_th + magcvr_info->detect_offset;
		magcvr_info->negative_far_noise_th = magcvr_info->negative_ori_far_noise_th - magcvr_info->detect_offset;

		MAG_CVR_LOG("cali-> farTh[%d,%d] noiTh[%d,%d] offset[%d]\n",
                magcvr_info->negative_far_threshold,
                magcvr_info->far_threshold,
                magcvr_info->negative_far_noise_th,
                magcvr_info->far_noise_th,
                magcvr_info->detect_offset);

		ret = magnetic_cover_get_data(magcvr_info);
		ret = magcvr_set_position(magcvr_info);
		ret = magcvr_set_threshold(magcvr_info);
	} else {
		MAG_CVR_ERR("fail\n");
	}

	return count;
}

// proc control for :: proc_cur_state_read
static ssize_t proc_cur_state_read(struct file *file, char __user *user_buf,
		size_t count, loff_t *ppos)
{
	struct magnetic_cover_info *magcvr_info = PDE_DATA(file_inode(file));

	int ret = 0;
	int value = 0;
	char page[6] = {0};

	if (!magcvr_info) {
		MAG_CVR_ERR("g_magcvr_info null\n");
		snprintf(page, 6, "%d\n", -1);
		return ret;
	} else {
		MAG_CVR_DEBUG("call");
		value = magnetic_cover_get_data(magcvr_info);
		snprintf(page, 6, "%d\n", value);
	}
	ret = simple_read_from_buffer(user_buf, count, ppos, page, strlen(page));
	return ret;
}

// proc control for :: proc_magcvr_healthinfo_read
static int proc_magcvr_healthinfo_read(struct seq_file *s, void *v)
{
	struct magnetic_cover_info *magcvr_info = s->private;
	int ret = 0;
	MAG_CVR_LOG("call");

	if (!magcvr_info) {
		MAG_CVR_ERR("magcvr_info null\n");
		seq_printf(s, "-1\n");
		return ret;
	}

	if (!magcvr_info->mc_ops) {
		MAG_CVR_ERR("mc_ops null\n");
		seq_printf(s, "-1\n");
		return ret;
	}

	// err feedback
	seq_printf(s, "iic_err:write[%d]read[%d]\n",
		magcvr_info->iic_write_err_cnt,
		magcvr_info->iic_read_err_cnt);

	seq_printf(s, "call_offset:%d$%d$%d\n",
			magcvr_info->cal_offset[0],
			magcvr_info->cal_offset[1],
			magcvr_info->cal_offset[CAL_OFFSET_MAX_CNT - 1]);

	seq_printf(s, "chip_ic_err:%d\n", magcvr_info->init_chip_failed);

	// state feedback
	return ret;
}

// proc control for :: proc_magcvr_healthinfo_write
// bit0 cmdd
// bit1 contrl
// 1 011 1 001

static ssize_t proc_magcvr_healthinfo_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *ppos)
{
	struct magnetic_cover_info *magcvr_info = PDE_DATA(file_inode(file));

	int data = 0;
	char temp[128] = {0};
	char *token = NULL;
	char *running = NULL;
	int ret = 0;

	MAG_CVR_LOG("call.\n");
	if (!magcvr_info) {
		MAG_CVR_ERR("g_magcvr_info null\n");
		return count;
	}

	ret = copy_from_user(temp, buffer, count);
	if (ret) {
		MAG_CVR_ERR("read proc input error.\n");
		return count;
	}

	temp[count-ret-1] = '\0';
	running = temp;
	MAG_CVR_DEBUG("end->temp:%s,count:%zu\n", temp, count);
	token = strsep(&running, CHECK_SEPARATOR);

	if (token != NULL) {
		fault_injection_check(magcvr_info, token);
		MAG_CVR_LOG("opt:%d,token:%s\n", magcvr_info->fault_injection_opt, token);
		token = strsep(&running, CHECK_SEPARATOR);
		if ((token != NULL) && (sscanf(token, "%d", &data))) {
			MAG_CVR_LOG("PASS:data:%d\n", data);
			other_opt_handle(magcvr_info, data);
		} else {
			MAG_CVR_LOG("FAIL:token:%s\n", token);
		}
	}

	return count;
}
// proc end

static int magcvr_parse_dts(struct device *dev,
                            struct magnetic_cover_info *magcvr_info)
{
	int val = 0;
	int rc = 0;
	struct device_node *m_node = NULL;

	if (dev == NULL) {
		MAG_CVR_ERR("call failed\n");
		return -EINVAL;
	} else {
		MAG_CVR_LOG("call success\n");
	}

	m_node = dev->of_node;
	// get hardware init parameter
	rc = of_property_read_u32(m_node, "magcvr_detect_step", &val);
	if (rc) {
		MAG_CVR_ERR("use distance_calib default.\n");
		magcvr_info->detect_step = M_DEF_SETP;
	} else {
		magcvr_info->detect_step = val;
	}
	MAG_CVR_LOG("magcvr_detect_step is %d\n", magcvr_info->detect_step);

	// Determine the positive or negative trigger mode
	rc = of_property_read_u32(m_node, "magcvr_pos_or_neg", &val);
	if (rc) {
		MAG_CVR_ERR("use distance_calib default.\n");
		magcvr_info->magcvr_pos_or_neg = M_BILATERAL;
	} else {
		magcvr_info->magcvr_pos_or_neg = val;
	}
	MAG_CVR_LOG("magcvr_pos_or_neg is %d\n", magcvr_info->magcvr_pos_or_neg);

	// magcvr_farmax_th
	rc = of_property_read_u32(m_node, "magcvr_farmax_th", &val);
	if (rc) {
		MAG_CVR_ERR("use magcvr_farmax_th default.\n");
		magcvr_info->far_max_th = M_FAR_MAX_TH;
	} else {
		magcvr_info->far_max_th = val;
	}
	MAG_CVR_LOG("far_max_th is %d\n", magcvr_info->far_max_th);

	// magcvr_irq_type
	rc = of_property_read_u32(m_node, "magcvr_irq_type", &val);
	if (rc) {
		MAG_CVR_ERR("use magcvr_irq_type default.\n");
		magcvr_info->irq_type = EDGE_DOWN;
	} else {
		magcvr_info->irq_type = val;
	}
	MAG_CVR_LOG("irq_type is %d\n", magcvr_info->irq_type);

	// magcvr_notify_support
	magcvr_info->magcvr_notify_support = of_property_read_bool(m_node, "magcvr_notify_support");
	MAG_CVR_LOG("magcvr_notify_support is %d\n", magcvr_info->magcvr_notify_support);

	// magcvr_far_threshold
	rc = of_property_read_u32(m_node, "magcvr_far_threshold", &val);
	if (rc) {
		MAG_CVR_ERR("use magcvr_far_threshold default.\n");
		magcvr_info->far_threshold = M_FAR_THRESHOLD;
		magcvr_info->negative_far_threshold = -M_FAR_THRESHOLD;
	} else {
		magcvr_info->far_threshold = val;
		magcvr_info->ori_far_threshold = val;
		magcvr_info->negative_far_threshold = -val;
		magcvr_info->negative_ori_far_threshold = -val;
	}
	MAG_CVR_LOG("far_threshold[%d,%d]\n",
        magcvr_info->negative_far_threshold, magcvr_info->far_threshold);

	rc = of_property_read_u32(m_node, "magcvr_far_noise_threshold", &val);
	if (rc) {
		MAG_CVR_ERR("use distance_calib default.\n");
		magcvr_info->far_noise_th = M_FAR_NOISE;
		magcvr_info->negative_far_noise_th = -M_FAR_NOISE;
	} else {
		magcvr_info->far_noise_th = val;
		magcvr_info->ori_far_noise_th = val;
		magcvr_info->negative_ori_far_noise_th = -val;
	}
	MAG_CVR_LOG("far_noise_th[%d,%d]\n",
        magcvr_info->negative_far_noise_th, magcvr_info->far_noise_th);

	// get hardware core config
	magcvr_info->irq_gpio = of_get_named_gpio(m_node, M_IRQ, 0);

	if (gpio_is_valid(magcvr_info->irq_gpio)) {
		MAG_CVR_LOG("gpio[%d] success get \n", magcvr_info->irq_gpio);
	} else {
		MAG_CVR_ERR("%s not specified in dts\n", M_IRQ);
	}

	rc = of_property_read_u32(m_node,
	                          M_1P8_VOLT,
	                          &magcvr_info->vddi_volt);
	if (rc < 0) {
		magcvr_info->vddi_volt = M_1P8_DEFAULT;
		MAG_CVR_LOG("%s not config,set default[%u]\n",
                M_1P8_VOLT, magcvr_info->vddi_volt);
	} else {
		MAG_CVR_LOG("%s is %u\n", M_1P8_VOLT, magcvr_info->vddi_volt);
	}

	rc = of_property_read_u32(m_node,
	                          M_3P0_VOLT,
	                          &magcvr_info->vdd_volt);
	if (rc < 0) {
		magcvr_info->vdd_volt = M_3P0_DEFAULT;
		MAG_CVR_ERR("%s not config,set default[%u]\n",
                M_3P0_VOLT, magcvr_info->vdd_volt);
	} else {
		MAG_CVR_LOG("%s is %u\n", M_3P0_VOLT, magcvr_info->vdd_volt);
	}

	magcvr_info->noise_set_threshold =
	    of_property_read_bool(m_node, "noise_not_set_threshold");
	MAG_CVR_LOG("noise_set_threshold=%d\n", magcvr_info->noise_set_threshold);

	magcvr_info->update_first_position =
	    of_property_read_bool(m_node, "update_first_position");
	MAG_CVR_LOG("update_first_position=%d\n", magcvr_info->update_first_position);


	magcvr_info->vddi = regulator_get(magcvr_info->magcvr_dev, M_1P8_NAME);
	if (IS_ERR_OR_NULL(magcvr_info->vddi)) {
		MAG_CVR_ERR("Regulator get failed %s\n", M_1P8_NAME);
	} else {
		MAG_CVR_LOG("Regulator get success %s\n", M_1P8_NAME);
	}

	magcvr_info->avdd = regulator_get(magcvr_info->magcvr_dev, M_3P0_NAME);
	if (IS_ERR_OR_NULL(magcvr_info->avdd)) {
		MAG_CVR_ERR("Regulator get failed %s\n", M_3P0_NAME);
	} else {
		MAG_CVR_LOG("Regulator get success %s\n", M_3P0_NAME);
	}

	return 0;
}

static void magcvr_handle_work(struct work_struct *work)
{
	struct magnetic_cover_info *magcvr_info = container_of(work,
                                        struct magnetic_cover_info,
                                        magcvr_handle_wq);
	int ret = 0;

	if (magcvr_info == NULL) {
		MAG_CVR_ERR("magcvr_get_data_work_func NULL \n");
		return;
	} else {
		MAG_CVR_LOG("work start\n");
	}

	mutex_lock(&magcvr_info->mutex);
	ret = magnetic_cover_get_data(magcvr_info);
	ret = magcvr_set_position(magcvr_info);
	ret = magcvr_set_threshold(magcvr_info);
	mutex_unlock(&magcvr_info->mutex);

	MAG_CVR_LOG("work end\n");
	return;
}

static int proc_magcvr_config_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_magcvr_config_read, PDE_DATA(inode));
}

static int proc_magcvr_farmax_th_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_magcvr_farmax_th_read, PDE_DATA(inode));
}

static int proc_magcvr_healthinfo_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_magcvr_healthinfo_read, PDE_DATA(inode));
}

DECLARE_PROC_OPS(proc_magcvr_config_ops,
    proc_magcvr_config_open,
    seq_read,
    NULL,
    single_release);

DECLARE_PROC_OPS(proc_data_handle_ops,
    simple_open,
    proc_cur_state_read,
    proc_distance_calib_write,
    NULL);

DECLARE_PROC_OPS(proc_magcvr_farmax_th_ops,
    proc_magcvr_farmax_th_open,
    seq_read,
    NULL,
    single_release);

DECLARE_PROC_OPS(proc_magcvr_healthinfo_ops,
    proc_magcvr_healthinfo_open,
    seq_read,
    proc_magcvr_healthinfo_write,
    NULL);

int interface_for_proc_init(struct magnetic_cover_info *magcvr_info)
{
	struct proc_dir_entry *prEntry_magcvr     = NULL;
	char magcvr_name[MAGCVR_NAME_SIZE_MAX];
	int i = 0;

	magcvr_proc_node magcvr_proc_node[] = {
        {
            "magcvr_config_para",
            CHMOD, NULL,
            &proc_magcvr_config_ops,
            magcvr_info,
            false,
            true
        },
        {
            "magcvr_farmax_th",
            CHMOD, NULL,
            &proc_magcvr_farmax_th_ops,
            magcvr_info,
            false,
            true
        },
        {
            "magcvr_data_handle",
            CHMOD,
            NULL,
            &proc_data_handle_ops,
            magcvr_info,
            false,
            true
        },
        {
            "magcvr_healthinfo",
            CHMOD,
            NULL,
            &proc_magcvr_healthinfo_ops,
            magcvr_info,
            false,
            true
        },
	};

	// proc/magnetic_cover start
	snprintf(magcvr_name, MAGCVR_NAME_SIZE_MAX, "%s", PROC_MAGCVR);
	MAG_CVR_DEBUG("create [/proc/%s] start\n", magcvr_name);
	prEntry_magcvr = proc_mkdir(magcvr_name, NULL);
	if (prEntry_magcvr == NULL) {
		MAG_CVR_LOG("ERR!!create magcvr proc entry\n");
		return 0;
	}
	magcvr_info->prEntry_magcvr = prEntry_magcvr;

	for (i = 0; i < ARRAY_SIZE(magcvr_proc_node); i++) {
		if (magcvr_proc_node[i].is_support) {
			magcvr_proc_node[i].node =
                proc_create_data(magcvr_proc_node[i].name,
                                magcvr_proc_node[i].mode,
                                prEntry_magcvr,
                                magcvr_proc_node[i].fops,
                                magcvr_proc_node[i].data);
			if (magcvr_proc_node[i].node == NULL) {
				magcvr_proc_node[i].is_created = false;
				MAG_CVR_ERR("create [proc/%s/%s] faild\n",
                    magcvr_name, magcvr_proc_node[i].name);
				return 0;
			} else {
				magcvr_proc_node[i].is_created = true;
				MAG_CVR_LOG("create [proc/%s/%s] success\n",
                    magcvr_name, magcvr_proc_node[i].name);
			}
		}
	}

	MAG_CVR_DEBUG("create [/proc/%s] end\n", magcvr_name);
	return 0;
}

static irqreturn_t magnetic_cover_irq_handler(int irq, void *dev_id)
{
	struct magnetic_cover_info *magcvr_info = (struct magnetic_cover_info *)dev_id;

	if (magcvr_info == NULL) {
		MAG_CVR_ERR(" magcvr_info==NULL,do not start work\n");
		return IRQ_NONE;
	}

	schedule_work(&magcvr_info->magcvr_handle_wq);
	MAG_CVR_LOG("call [irq:%d]value:%d\n", irq, magcvr_info->m_value);

	return IRQ_HANDLED;
}

static int interface_for_input_init(struct magnetic_cover_info *magcvr_info)
{
	int ret = 0;

	magcvr_info->input_dev = input_allocate_device();
	if (magcvr_info->input_dev == NULL) {
		ret = -ENOMEM;
		MAG_CVR_ERR("Failed to allocate input device\n");
		return -1;
	}
	magcvr_info->input_dev->name = MAGNETIC_COVER_INPUT_DEVICE;

	set_bit(EV_SYN, magcvr_info->input_dev->evbit);
	set_bit(EV_KEY, magcvr_info->input_dev->evbit);
	set_bit(KEY_F6, magcvr_info->input_dev->keybit);
	set_bit(KEY_F7, magcvr_info->input_dev->keybit);
	ret = input_register_device(magcvr_info->input_dev);
	if (ret) {
		MAG_CVR_ERR("Failed to register input device\n");
		input_free_device(magcvr_info->input_dev);
		return -1;
	}
	return 0;
}

static int magcvr_interface_register(struct magnetic_cover_info *magcvr_info)
{
	int ret = 0;

	ret = interface_for_proc_init(magcvr_info);
	if (ret < 0) {
		MAG_CVR_ERR("create interface_for_proc_init fail\n");
	} else {
		MAG_CVR_DEBUG("create interface_for_proc_init success\n");
	}

	ret = interface_for_input_init(magcvr_info);
	if (ret < 0) {
		MAG_CVR_ERR("create interface_for_input_init fail\n");
	} else {
		MAG_CVR_DEBUG("create interface_for_input_init success\n");
	}
	return 0;
}

int magcvr_set_threshold(struct magnetic_cover_info *magcvr_info)
{
	int ret = 0;
	int position = 0;
	int high_thd = 0;
	int low_thd  = 0;

	if (magcvr_info == NULL) {
		MAG_CVR_ERR("magcvr_info == NULL\n");
		return ret;
	}

	position = magcvr_info->position;
	high_thd = magcvr_info->high_thd;
	low_thd  = magcvr_info->low_thd;

	if (magcvr_info->same_position == true &&
		magcvr_info->noise_set_threshold == true) {
		magcvr_info->same_position = false;
		high_thd = magcvr_info->m_value + NOISE_STEP;
		low_thd  = magcvr_info->m_value - NOISE_STEP;
	} else {
		high_thd = magcvr_info->m_value + magcvr_info->detect_step;
		low_thd  = magcvr_info->m_value - magcvr_info->detect_step;
	}

	MAG_CVR_LOG("threshold[h:%d][l:%d][value:%d][step:%d][position:%d][last position:%d]\n",
		high_thd, low_thd, magcvr_info->m_value, magcvr_info->detect_step,
		position, magcvr_info->last_position);

	if (magcvr_info->mc_ops->update_threshold) {
		MAG_CVR_DEBUG("update_threshold ops exist\n");
		ret = magcvr_info->mc_ops->update_threshold( \
                        magcvr_info->chip_info, position, high_thd, low_thd);
		if (ret < 0 || fault_injection_handle(magcvr_info, OPT_IIC_WRITE)) {
			MAG_CVR_ERR("update_threshold faild\n");
#if IS_ENABLED(CONFIG_OPLUS_MAGCVR_NOTIFY)
			mag_call_notifier(MAGNETIC_COVER_IIC_FAIL);
#endif
		}
	} else {
		MAG_CVR_ERR("update_threshold api not found\n");
	}

	return ret;
}

static int magcvr_update_first_position(struct magnetic_cover_info *magcvr_info)
{
	int ret = 0;
	long value = 0;
	MAG_CVR_LOG("not update_threshold ops,somthing is wrong\n");
	ret = magcvr_info->mc_ops->get_data(magcvr_info->chip_info, &value);
	return 0;
}

#if IS_ENABLED(CONFIG_OPLUS_MAGCVR_NOTIFY)
void mag_call_notifier(int position)
{
	struct magcvr_notify_event event_data;

	memset(&event_data, 0, sizeof(struct magcvr_notify_event));
	switch (position) {
		case MAGNETIC_COVER_INPUT_NEAR:
			event_data.type = MAGCVR_CALL_NEAR;
		break;
		case MAGNETIC_COVER_INPUT_FAR:
			event_data.type = MAGCVR_CALL_FAR;
		break;
		case MAGNETIC_COVER_IIC_FAIL:
			event_data.type = MAGCVR_CALL_IIC_FAIL;
		break;
		case MAGNETIC_COVER_IC_FAIL:
			event_data.type = MAGCVR_CALL_IC_FAIL;
		break;
	}

	event_data.type = position;
	MAG_CVR_LOG("[transfer:nofity] posi->%d\n", event_data.type);
	magcvr_event_call_notifier(EVENT_ACTION_FOR_MAGCVR, (void *)&event_data);
}
#endif

int magcvr_set_position(struct magnetic_cover_info *magcvr_info)
{
	int ret = 0;

	if (magcvr_info == NULL) {
		MAG_CVR_ERR("magcvr_info == NULL\n");
		return ret;
	}

	MAG_CVR_LOG("value[%d] thres[neg:%d pos:%d] noise[neg:%d pos:%d] [state:%d]\n",
        magcvr_info->m_value,
        magcvr_info->negative_far_threshold,
        magcvr_info->far_threshold,
        magcvr_info->negative_far_noise_th,
        magcvr_info->far_noise_th,
        magcvr_info->magcvr_pos_or_neg);

	magcvr_info->same_position = false;

	if (magcvr_info->iic_read_fail > 0) {
		MAG_CVR_ERR("iic_read_fail,not set position\n");
#if IS_ENABLED(CONFIG_OPLUS_MAGCVR_NOTIFY)
		mag_call_notifier(MAGNETIC_COVER_IIC_FAIL);
#endif
		magcvr_info->iic_read_fail = 0;
		return ret;
	}

	if (magcvr_info->m_value >=0 &&
        (magcvr_info->magcvr_pos_or_neg == M_POSITIVE | magcvr_info->magcvr_pos_or_neg == M_BILATERAL)) {
		if (magcvr_info->m_value > magcvr_info->far_threshold) {
			magcvr_info->position = MAGNETIC_COVER_INPUT_NEAR;
			MAG_CVR_LOG("[POS]-->NEAR\n");
		} else if (magcvr_info->m_value <= magcvr_info->far_threshold &&
			magcvr_info->m_value >= magcvr_info->far_noise_th){
			magcvr_info->position = magcvr_info->last_position;
			magcvr_info->same_position = true;
			MAG_CVR_LOG("[POS]position=last position[%d]\n", magcvr_info->last_position);
		} else {
			magcvr_info->position = MAGNETIC_COVER_INPUT_FAR;
			MAG_CVR_LOG("[POS]-->FAR\n");
		}
	} else if (magcvr_info->m_value < 0 &&
        (magcvr_info->magcvr_pos_or_neg == M_NEGATIVE | magcvr_info->magcvr_pos_or_neg == M_BILATERAL)) {
		if (magcvr_info->m_value < magcvr_info->negative_far_threshold) {
			magcvr_info->position = MAGNETIC_COVER_INPUT_NEAR;
			MAG_CVR_LOG("[NEG]-->NEAR\n");
		} else if (magcvr_info->m_value >= magcvr_info->negative_far_threshold &&
			magcvr_info->m_value <= magcvr_info->negative_far_noise_th){
			magcvr_info->position = magcvr_info->last_position;
			MAG_CVR_LOG("[NEG]position=last position[%d]\n", magcvr_info->last_position);
		} else {
			magcvr_info->position = MAGNETIC_COVER_INPUT_FAR;
			MAG_CVR_LOG("[NEG]-->FAR\n");
		}
	}

	magcvr_info->last_position = magcvr_info->position;

	if (magcvr_info->driver_start == true) {
		MAG_CVR_LOG("driver probe,no need report position\n");
		return ret;
	}

	if (!magcvr_info->magcvr_notify_support) {
#if IS_ENABLED(CONFIG_OPLUS_MAGCVR_NOTIFY)
		mag_call_notifier(magcvr_info->position);
#endif
	}
	return ret;
}

static int magcvr_init_something(struct magnetic_cover_info *magcvr_info)
{
	int ret = 0;
	int i = 0;

	if (magcvr_info == NULL) {
		MAG_CVR_ERR("magcvr_info == NULL\n");
		return ret;
	}

	MAG_CVR_LOG("init mutex\n");
	mutex_init(&magcvr_info->mutex);
	// delay thread init
	MAG_CVR_LOG("init work\n");
	INIT_WORK(&magcvr_info->magcvr_handle_wq, magcvr_handle_work);

	debug_enable = false;

	// healthinfo init
	magcvr_info->iic_read_err_cnt= 0;
	magcvr_info->iic_write_err_cnt= 0;
	magcvr_info->cal_offset_cnt= 0;
	magcvr_info->iic_read_fail = 0;
	magcvr_info->iic_write_fail = 0;

	for (i = 0; i < ERR_MAG_REG_MAX_CNT; i++)
		magcvr_info->reg_err[i] = 0;
	for (i = 0; i < CAL_OFFSET_MAX_CNT; i++)
		magcvr_info->cal_offset[i] = 0;

	magcvr_info->init_chip_failed = false;
	magcvr_info->same_position    = false;
	magcvr_info->noise_set_threshold = false;
	magcvr_info->update_first_position   = false;

	return ret;
}

int magcvr_powercontrol(struct magnetic_cover_info *magcvr_info,
                             bool enable,
                             int power_type)
{
	int ret = 0;

	if (magcvr_info == NULL) {
		MAG_CVR_ERR("magcvr_info == NULL\n");
		return ret;
	}

	MAG_CVR_LOG("The %s Regulator %s.\n",
		power_type > 0 ? "M_3P0" : "M_1P8",
		enable ? "enable" : "disable");

	switch (power_type) {
		case M_1P8:
			if (!IS_ERR_OR_NULL(magcvr_info->vddi)) {
				MAG_CVR_DEBUG("Enable the Regulator vddi.\n");
				if (!!enable) {
					ret = regulator_enable(magcvr_info->vddi);
					if (ret) {
						MAG_CVR_ERR("Regulator 1.8 enable failed ret = %d\n", ret);
						return ret;
					}
				} else {
					ret = regulator_disable(magcvr_info->vddi);
					if (ret) {
						MAG_CVR_ERR("Regulator 1.8 disable failed ret = %d\n", ret);
						return ret;
					}
				}
			}
		break;
		case M_3P0:
			if (!IS_ERR_OR_NULL(magcvr_info->avdd)) {
				MAG_CVR_DEBUG("Enable the Regulator avdd.\n");
				if (!!enable) {
					ret = regulator_enable(magcvr_info->avdd);
					if (ret) {
						MAG_CVR_ERR("Regulator 3.0 enable failed ret = %d\n", ret);
						return ret;
					}
				} else {
					ret = regulator_disable(magcvr_info->avdd);
					if (ret) {
						MAG_CVR_ERR("Regulator 3.0 enable disable ret = %d\n", ret);
						return ret;
					}
				}
			}
		break;
	}

	return 0;
}
EXPORT_SYMBOL(magcvr_powercontrol);

static int magcvr_power_set(struct magnetic_cover_info *magcvr_info, bool status)
{
	int ret = 0;

	if (magcvr_info == NULL) {
		MAG_CVR_ERR("magcvr_info == NULL\n");
		return ret;
	}

	// set 1.8v
	if (regulator_count_voltages(magcvr_info->vddi) > 0) {
		if (magcvr_info->vddi_volt) {
			MAG_CVR_LOG("set vddi_volt[%d]uV\n", magcvr_info->vddi_volt);
			ret = regulator_set_voltage(magcvr_info->vddi,
                                        magcvr_info->vddi_volt,
                                        magcvr_info->vddi_volt);
			if (ret) {
				MAG_CVR_ERR("Regulator set_vtg failed vcc_i2c rc = %d\n", ret);
				return -1;
			}
		}
		ret = regulator_set_load(magcvr_info->vddi, M_VOLT_LOAD);
		if (ret < 0) {
			MAG_CVR_ERR("Failed to set vcc_1v8 mode(rc:%d)\n", ret);
			return -1;
		}
	} else {
		MAG_CVR_ERR("1P8 regulator_count_voltages is not support\n");
	}

	// set 3.0v
	if (regulator_count_voltages(magcvr_info->avdd) > 0) {
		MAG_CVR_LOG("set vdd_volt[%d]uV\n", magcvr_info->vdd_volt);
		if (magcvr_info->vdd_volt) {
			ret = regulator_set_voltage(magcvr_info->avdd,
                                        magcvr_info->vdd_volt,
                                        magcvr_info->vdd_volt);
			if (ret) {
				MAG_CVR_ERR("Regulator set_vtg failed vdd rc = %d\n", ret);
				return -1;
			}
		}
		ret = regulator_set_load(magcvr_info->avdd, M_VOLT_LOAD);
		if (ret < 0) {
			MAG_CVR_ERR("Failed to set vdd_2v8 mode(rc:%d)\n", ret);
			return -1;
		}
	} else {
		MAG_CVR_ERR("3V0 regulator_count_voltages is not support\n");
	}

	// enable power
	ret = magcvr_powercontrol(magcvr_info, POWER_ENABLE, M_3P0);
	ret = magcvr_powercontrol(magcvr_info, POWER_ENABLE, M_1P8);

return ret;
}


struct magnetic_cover_info *alloc_for_magcvr(void)
{
	void *alloc;

	alloc = kzalloc(sizeof(struct magnetic_cover_info), GFP_KERNEL);
	if (!alloc) {
		MAG_CVR_ERR("Failed to allocate memory\n");
		/*add for health monitor*/
	} else {
		MAG_CVR_LOG("success to allocate memory size(%ld)\n", sizeof(struct magnetic_cover_info));
	}

	return alloc;
}
EXPORT_SYMBOL(alloc_for_magcvr);

static int magcvr_chip_init(struct magnetic_cover_info *magcvr_info)
{
	int ret = 0;

	if (magcvr_info == NULL) {
		MAG_CVR_ERR("g_chip NULL \n");
		return -1;
	}

	if (magcvr_info->mc_ops->chip_init) {
		ret = magcvr_info->mc_ops->chip_init(magcvr_info->chip_info);
		if (ret < 0) {
			MAG_CVR_ERR("chip_init fail, chip abnormal!!\n");
			magcvr_info->init_chip_failed = true;
		}
	} else {
		MAG_CVR_ERR("magcvr_info->mc_ops->chip_init is NULL\n");
	}

	return ret;
}

int magcvr_setup_eint(struct magnetic_cover_info *magcvr_info)
{
	int ret = 0;
	unsigned long irqflags = 0;

	MAG_CVR_LOG("called");
	if (gpio_is_valid(magcvr_info->irq_gpio)) {
		ret = devm_gpio_request(magcvr_info->magcvr_dev, magcvr_info->irq_gpio, M_IRQ);
		if (ret) {
			MAG_CVR_ERR("unable %s to request gpio [%d]\n", M_IRQ, magcvr_info->irq_gpio);
		} else {
			MAG_CVR_LOG("gpio[%d] set success \n", magcvr_info->irq_gpio);
		}
		ret = gpio_direction_input(magcvr_info->irq_gpio);
		msleep(50);
		magcvr_info->irq = gpio_to_irq(magcvr_info->irq_gpio);
		ret = 0;
	} else {
		magcvr_info->irq = -EINVAL;
		MAG_CVR_ERR("irq_gpio is invalid\n");
		ret = -EINVAL;
	}

	if (magcvr_info->irq_type == EDGE_DOWN) {
		irqflags = (IRQ_TYPE_EDGE_FALLING | IRQF_ONESHOT);
		MAG_CVR_LOG("EDGE_FALLING->[GPIO:%d] [irq:%d]\n", magcvr_info->irq_gpio, magcvr_info->irq);
	} else if (magcvr_info->irq_type == LOW_LEVEL) {
		irqflags = (IRQ_TYPE_LEVEL_LOW | IRQF_ONESHOT);
		MAG_CVR_LOG("LEVEL_LOW->[GPIO:%d] [irq:%d]\n", magcvr_info->irq_gpio, magcvr_info->irq);
	}

	/* kernel_platform/common/include/linux/interrupt.h +40 */
	if (magcvr_info->irq > 0) {
		ret = devm_request_threaded_irq(magcvr_info->magcvr_dev,
                                        magcvr_info->irq,
                                        NULL,
                                        magnetic_cover_irq_handler,
                                        irqflags,
                                        M_INTERRUPT,
                                        magcvr_info);

		if (ret < 0) {
			MAG_CVR_ERR("IRQ LINE NOT AVAILABLE!!\n");
			return -EINVAL;
		} else {
			MAG_CVR_LOG("set interrupt handle success\n");
			irq_set_irq_wake(magcvr_info->irq, 1);
		}
	}

	MAG_CVR_DEBUG("first disable irq, and enable if platform need\n");
	disable_irq(magcvr_info->irq);
	return ret;
}

int magcvr_core_init(struct magnetic_cover_info *magcvr_data)
{
	struct magnetic_cover_info *magcvr_info = magcvr_data;
	int ret = 0;

	if (magcvr_info == NULL) {
		MAG_CVR_ERR("call failed\n");
		return -1;
	} else {
		MAG_CVR_LOG("call success\n");
		magcvr_info->driver_start = false;
	}

	ret = magcvr_init_something(magcvr_info);
	if (ret < 0) {
		MAG_CVR_ERR("magcvr_init_other err!!\n");
		goto FAIL;
	} else {
		MAG_CVR_DEBUG("magcvr_init_other success\n");
	}


	ret = magcvr_parse_dts(magcvr_info->magcvr_dev, magcvr_info);
	if (ret < 0) {
		MAG_CVR_ERR("magcvr_parse_dts err!!\n");
		goto FAIL;
	} else {
		MAG_CVR_DEBUG("magcvr_parse_dts success\n");
	}

	ret = magcvr_power_set(magcvr_info, POWER_ENABLE);
	if (ret < 0) {
		MAG_CVR_ERR("magcvr_power_set err!!\n");
		goto FAIL;
	} else {
		MAG_CVR_DEBUG("magcvr_power_set success\n");
	}

	ret = magcvr_interface_register(magcvr_info);
	if (ret < 0) {
		MAG_CVR_ERR("magcvr_interface_register!!\n");
		goto FAIL;
	} else {
		MAG_CVR_DEBUG("magcvr_interface_register success\n");
	}

	ret = magcvr_chip_init(magcvr_info);
	if (ret < 0) {
		MAG_CVR_ERR("magcvr_chip_init err!!\n");
		goto INIT_ERR;
	} else {
		MAG_CVR_DEBUG("magcvr_chip_init success\n");
	}

	ret = magcvr_setup_eint(magcvr_info);
	if (ret < 0) {
		MAG_CVR_ERR("magcvr_setup_eint err!!\n");
		goto FAIL;
	} else {
		MAG_CVR_DEBUG("magcvr_setup_eint success\n");
	}

	ret = magnetic_cover_get_data(magcvr_info);
	MAG_CVR_DEBUG("magnetic_cover_get_data is %d\n", ret);

	ret = magcvr_set_position(magcvr_info);
	if (ret < 0) {
		MAG_CVR_ERR("magcvr_set_position err!!\n");
		goto FAIL;
	} else {
		MAG_CVR_DEBUG("magcvr_set_position success\n");
	}

	ret = magcvr_set_threshold(magcvr_info);
	if (ret < 0) {
		MAG_CVR_ERR("magcvr_init_threshold err!!\n");
		goto FAIL;
	} else {
		MAG_CVR_DEBUG("magcvr_init_threshold success\n");
	}

	magcvr_info->driver_start = false;
	// enable irq
	enable_irq(magcvr_info->irq);
	MAG_CVR_LOG(" all success. abstract end, enable irq:%d\n", magcvr_info->irq);
	return 0;

INIT_ERR:
	MAG_CVR_LOG(" all success. abstract end, enable irq:%d\n", magcvr_info->irq);
	return INIT_PROBE_ERR;
FAIL:
	MAG_CVR_ERR("fail. \n");
	return -ENXIO;
}
EXPORT_SYMBOL(magcvr_core_init);

int after_magcvr_core_init(struct magnetic_cover_info *magcvr_info)
{
	int ret = 0;

	if (magcvr_info->update_first_position) {
		ret = magcvr_update_first_position(magcvr_info);
		if (ret < 0) {
			MAG_CVR_ERR("no need update first position\n");
		} else {
			MAG_CVR_DEBUG("update first position success\n");
		}
	}

	if (magcvr_info->init_chip_failed) {
		MAG_CVR_LOG("some err,must send err flag to clinet!!\n");
#if IS_ENABLED(CONFIG_OPLUS_MAGCVR_NOTIFY)
		mag_call_notifier(MAGNETIC_COVER_IC_FAIL);
#endif
	}

	return ret;
}
EXPORT_SYMBOL(after_magcvr_core_init);

void unregister_magcvr_core(struct magnetic_cover_info *magcvr_info)
{
	if (magcvr_info == NULL) {
		MAG_CVR_ERR("magcvr_info == NULL\n");
		return;
	}
	// free proc
	if (magcvr_info->prEntry_magcvr) {
		remove_proc_subtree(PROC_MAGCVR, NULL);
	}
	// free power
	// reserved
	// free mutex
	mutex_destroy(&magcvr_info->mutex);
	// free wrok
	cancel_work_sync(&magcvr_info->magcvr_handle_wq);
	// free resource
	kfree(magcvr_info);
}
EXPORT_SYMBOL(unregister_magcvr_core);

MODULE_DESCRIPTION("Magentic cover Driver");
MODULE_LICENSE("GPL");
