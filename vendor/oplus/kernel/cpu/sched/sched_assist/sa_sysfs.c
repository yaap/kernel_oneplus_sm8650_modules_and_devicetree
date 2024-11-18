// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 Oplus. All rights reserved.
 */


#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/sched/cputime.h>
#include <linux/ioctl.h>
#include <kernel/sched/sched.h>

#include "sa_common.h"
#include "sa_sysfs.h"
#ifdef CONFIG_OPLUS_CPU_AUDIO_PERF
#include "sa_audio.h"
#endif

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_LOADBALANCE)
#include "sa_balance.h"
#endif

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_PIPELINE)
#include "sa_pipeline.h"
#endif

#define OPLUS_SCHEDULER_PROC_DIR		"oplus_scheduler"
#define OPLUS_SCHEDASSIST_PROC_DIR		"sched_assist"

#define MAX_SET (128)
#define MAX_THREAD_INPUT (6)

#define UX_MAGIC 0x27
#define CMD_UX_READ  _IOR(UX_MAGIC, 0, int[2])
#define CMD_UX_WRITE _IOW(UX_MAGIC, 1, int[3])

int global_debug_enabled;
EXPORT_SYMBOL(global_debug_enabled);
int global_sched_assist_enabled;
EXPORT_SYMBOL(global_sched_assist_enabled);
int global_sched_assist_scene;
EXPORT_SYMBOL(global_sched_assist_scene);

pid_t global_ux_task_pid = -1;
pid_t global_im_flag_pid = -1;

pid_t save_audio_tgid;
pid_t save_top_app_tgid;
unsigned int top_app_type;

struct proc_dir_entry *d_oplus_scheduler;
struct proc_dir_entry *d_sched_assist;

static int disable_setting = 1;
static int hint_message = -1;

#ifdef CONFIG_OPLUS_FEATURE_TICK_GRAN
static u64 last_total_instr;
static u64 last_total_ncsw;
static u64 last_total_nvcsw;
#endif

enum {
	OPT_STR_TYPE = 0,
	OPT_STR_PID,
	OPT_STR_VAL,
	OPT_STR_MAX = 3,
};

static ssize_t proc_debug_enabled_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	char buffer[8];
	int err, val;

	memset(buffer, 0, sizeof(buffer));

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	buffer[count] = '\0';
	err = kstrtoint(strstrip(buffer), 10, &val);
	if (err)
		return err;

	global_debug_enabled = val;

	return count;
}

static ssize_t proc_debug_enabled_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	char buffer[20];
	size_t len = 0;

	len = snprintf(buffer, sizeof(buffer), "debug_enabled=%d\n", global_debug_enabled);

	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t proc_sched_assist_enabled_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	char buffer[13];
	int err, val;

	memset(buffer, 0, sizeof(buffer));

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	buffer[count] = '\0';
	err = kstrtoint(strstrip(buffer), 10, &val);
	if (err)
		return err;

	global_sched_assist_enabled = val;

	return count;
}

static ssize_t proc_sched_assist_enabled_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	char buffer[13];
	size_t len = 0;

	len = snprintf(buffer, sizeof(buffer), "enabled=%d\n", global_sched_assist_enabled);

	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t proc_sched_assist_scene_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	char buffer[13];
	int err, val;
	static DEFINE_MUTEX(sa_scene_mutex);

	memset(buffer, 0, sizeof(buffer));

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	buffer[count] = '\0';
	err = kstrtoint(strstrip(buffer), 10, &val);
	if (err)
		return err;

	mutex_lock(&sa_scene_mutex);

	if (val == SA_SCENE_OPT_CLEAR) {
		global_sched_assist_scene = val;
		goto out;
	}

	if (val & SA_SCENE_OPT_SET)
		global_sched_assist_scene |= val & (~SA_SCENE_OPT_SET);
	else if (val & global_sched_assist_scene)
		global_sched_assist_scene &= ~val;

out:
	mutex_unlock(&sa_scene_mutex);
	return count;
}

static ssize_t proc_sched_assist_scene_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	char buffer[13];
	size_t len = 0;

	len = snprintf(buffer, sizeof(buffer), "scene=%d\n", global_sched_assist_scene);

	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

/*
 * Example:
 * adb shell "echo "p 1611 130" > proc/oplus_scheduler/sched_assist/ux_task"
 * 'p' means pid, '1611' is thread pid, '130' means '128 + 2', set ux state as '2'
 *
 * adb shell "echo "r 1611" > proc/oplus_scheduler/sched_assist/ux_task"
 * "r" means we want to read thread "1611"'s info
 */
static ssize_t proc_ux_task_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	char buffer[MAX_SET];
	char *str, *token;
	char opt_str[OPT_STR_MAX][13] = {"0", "0", "0"};
	int cnt = 0;
	int pid = 0;
	int ux_state = 0, ux_orig = 0;
	int err = 0;
	int is_hint_message = 0;
	static DEFINE_MUTEX(sa_ux_mutex);

	int uid = task_uid(current).val;

	if(uid % 100000 == hint_message)
		is_hint_message = 1;

	/* only accept ux from system server or performance binder */
	if (SYSTEM_UID != uid && ROOT_UID != uid && (!is_hint_message) && disable_setting) {
		return -EFAULT;
	}

	memset(buffer, 0, sizeof(buffer));

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	buffer[count] = '\0';
	str = strstrip(buffer);
	while ((token = strsep(&str, " ")) && *token && (cnt < OPT_STR_MAX)) {
		strlcpy(opt_str[cnt], token, sizeof(opt_str[cnt]));
		cnt += 1;
	}

	if (cnt != OPT_STR_MAX) {
		if (cnt == (OPT_STR_MAX - 1) && !strncmp(opt_str[OPT_STR_TYPE], "r", 1)) {
			err = kstrtoint(strstrip(opt_str[OPT_STR_PID]), 10, &pid);
			if (err)
				return err;

			if (pid > 0 && pid <= PID_MAX_DEFAULT)
				global_ux_task_pid = pid;
		}

		return -EFAULT;
	}

	err = kstrtoint(strstrip(opt_str[OPT_STR_PID]), 10, &pid);
	if (err)
		return err;

	err = kstrtoint(strstrip(opt_str[OPT_STR_VAL]), 10, &ux_state);
	if (err)
		return err;

	mutex_lock(&sa_ux_mutex);
	if (!strncmp(opt_str[OPT_STR_TYPE], "p", 1) && (ux_state >= 0)) {
		struct task_struct *ux_task = NULL;

		if (pid > 0 && pid <= PID_MAX_DEFAULT) {
			rcu_read_lock();
			ux_task = find_task_by_vpid(pid);
			if (ux_task)
				get_task_struct(ux_task);
			rcu_read_unlock();

			if (ux_task) {
				ux_orig = oplus_get_ux_state(ux_task);

				if ((ux_state & SA_OPT_SET) && oplus_get_inherit_ux(ux_task)) {
					clear_all_inherit_type(ux_task);
					ux_orig = 0;
				}

				if (ux_state == SA_OPT_CLEAR) { /* clear all ux type but animator type */
					if (ux_orig & SA_TYPE_ANIMATOR)
						ux_orig &= SA_TYPE_ANIMATOR;
					else
						ux_orig = 0;
					oplus_set_ux_state_lock(ux_task, ux_orig, -1, true);
				} else if (ux_state & SA_OPT_SET) { /* set target ux type and clear set opt */
					if (ux_state & SA_OPT_SET_PRIORITY) {
						ux_orig &= ~(SCHED_ASSIST_UX_PRIORITY_MASK);
					}
					ux_orig |= ux_state & ~(SA_OPT_SET|SA_OPT_SET_PRIORITY);
					oplus_set_ux_state_lock(ux_task, ux_orig, -1, true);
				} else if (ux_orig & ux_state) { /* reset target ux type */
					ux_orig &= ~ux_state;
					/* if ux_state->0 after clear ux bit, and it is inherited, should keep it */
					if (!(ux_orig & SCHED_ASSIST_UX_MASK) && (ux_orig & SA_TYPE_INHERIT)) {
						/* do nothing */
					} else {
						oplus_set_ux_state_lock(ux_task, ux_orig, -1, true);
					}
				}

				put_task_struct(ux_task);
			}
		}
	}

	mutex_unlock(&sa_ux_mutex);
	return count;
}

static ssize_t proc_ux_task_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	char buffer[256];
	size_t len;
	struct task_struct *task;

	rcu_read_lock();
	task = find_task_by_vpid(global_ux_task_pid);
	if (task)
		get_task_struct(task);
	rcu_read_unlock();

	if (task) {
		struct oplus_task_struct *ots;
		ots = get_oplus_task_struct(task);
		if (IS_ERR_OR_NULL(ots)) {
			len = snprintf(buffer, sizeof(buffer), "Ots is null\n");
		} else {
			len = snprintf(buffer, sizeof(buffer), "comm=%s pid=%d tgid=%d ux_state=0x%08x ux_prio=%d ux_nice=%d inherit=%llx(bi:%d rw:%d mu:%d) im_flag=0x%08lx\n",
				task->comm, task->pid, task->tgid, ots->ux_state, ots->ux_priority, ots->ux_nice, oplus_get_inherit_ux(task),
				test_inherit_ux(task, INHERIT_UX_BINDER), test_inherit_ux(task, INHERIT_UX_RWSEM), test_inherit_ux(task, INHERIT_UX_MUTEX),
				ots->im_flag);
		}
		put_task_struct(task);
	} else
		len = snprintf(buffer, sizeof(buffer), "Can not find task\n");

	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static int read_task_ux(pid_t pid, pid_t tid, bool fromSysOrApp)
{
	long ret = -1;
	struct task_struct *task;

	rcu_read_lock();
	task = find_task_by_vpid(tid);
	if (task) {
		if (task->tgid == pid) {
			struct oplus_task_struct *ots;
			ots = get_oplus_task_struct(task);
			if (IS_ERR_OR_NULL(ots)) {
				ret = -EFAULT;
			} else {
				bool verified;
				uid_t curr_uid = current_uid().val;

				if (fromSysOrApp) {
					/* permit system to control ux setting for any task */
					verified = (curr_uid == ROOT_UID || curr_uid == SYSTEM_UID);
				} else {
					/* permit system and app to access for same uid */
					curr_uid = curr_uid % PER_USER_RANGE;
					verified = (current->tgid == task->tgid) && ((curr_uid == SYSTEM_UID) ||
						((curr_uid >= FIRST_APPLICATION_UID) && (curr_uid <= LAST_APPLICATION_UID)));
				}

				if (verified) {
					ret = ots->ux_state;
				} else {
					ret = -EPERM;
				}
			}
		} else {
			ret = -EINVAL;
		}
	} else {
		ret = -ESRCH;
	}
	rcu_read_unlock();
	return ret;
}

static long write_task_ux(pid_t pid, pid_t tid, int ux_value, bool fromSysOrApp)
{
	long ret = -1;
	struct task_struct *ux_task, *task;
	int ux_orig;

	/* set and reset operation are mutual */
	if ((ux_value & SA_OPT_RESET) && (ux_value & SA_OPT_SET)) {
		return -EINVAL;
	}

	ux_task = NULL;

	rcu_read_lock();
	task = find_task_by_vpid(tid);
	if (task) {
		if (task->tgid == pid) {
			struct oplus_task_struct *ots;
			ots = get_oplus_task_struct(task);
			if (IS_ERR_OR_NULL(ots)) {
				ret = -EFAULT;
			}  else {
				bool verified;
				uid_t curr_uid = current_uid().val;

				if (fromSysOrApp) {
					/* permit system to control ux setting for any task */
					verified = (curr_uid == ROOT_UID || curr_uid == SYSTEM_UID);
				} else {
					/* permit system and app to access for same uid */
					curr_uid = curr_uid % PER_USER_RANGE;
					verified = (current->tgid == task->tgid) && ((curr_uid == SYSTEM_UID) ||
						((curr_uid >= FIRST_APPLICATION_UID) && (curr_uid <= LAST_APPLICATION_UID)));
				}

				if (verified) {
					ux_orig = ots->ux_state;
					ux_task = task;
					get_task_struct(ux_task);
				} else {
					ret = -EPERM;
				}
			}
		} else {
			ret = -EINVAL;
		}
	} else {
		ret = -ESRCH;
	}
	rcu_read_unlock();

	if (ux_task) {
		bool need_update = true;
		int ux_state = -1;

		/* clear inherit type if ux is intentional set */
		if ((ux_value & (SA_OPT_SET|SA_OPT_RESET)) && oplus_get_inherit_ux(ux_task)) {
			clear_all_inherit_type(ux_task);
		}

		if ((ux_value & (SA_OPT_RESET|SA_OPT_SET_PRIORITY)) == (SA_OPT_RESET|SA_OPT_SET_PRIORITY)) {
			/* reset ux and priority operation will overwrite current ux state */
			ux_state = ux_value & (SCHED_ASSIST_UX_PRIORITY_MASK|SCHED_ASSIST_UX_MASK);
		} else if (ux_value & SA_OPT_RESET) {
			/* reset ux operation only keep current ux priority */
			ux_orig &= SCHED_ASSIST_UX_PRIORITY_MASK;
			ux_state = (ux_value & SCHED_ASSIST_UX_MASK) | ux_orig;
		} else if ((ux_value & (SA_OPT_SET|SA_OPT_SET_PRIORITY)) == (SA_OPT_SET|SA_OPT_SET_PRIORITY)) {
			if ((ux_value & SCHED_ASSIST_UX_MASK) == SA_OPT_CLEAR) {
				/* clear all ux type but animator type */
				ux_state = ux_value & SCHED_ASSIST_UX_PRIORITY_MASK;
				ux_orig &= SA_TYPE_ANIMATOR;
				ux_state |= ux_orig;
			} else {
				/* union two ux type bit */
				ux_state = ux_value & (SCHED_ASSIST_UX_PRIORITY_MASK|SCHED_ASSIST_UX_MASK);
				ux_orig &= SCHED_ASSIST_UX_MASK;
				ux_state |= ux_orig;
			}
		} else if (ux_value & SA_OPT_SET) {
			if ((ux_value & SCHED_ASSIST_UX_MASK) == SA_OPT_CLEAR) {
				/* clear all ux type but animator type */
				ux_state = ux_orig & (SCHED_ASSIST_UX_PRIORITY_MASK|SA_TYPE_ANIMATOR);
			} else {
				/* union two ux type bit */
				ux_state = ux_value & SCHED_ASSIST_UX_MASK;
				ux_orig &= (SCHED_ASSIST_UX_PRIORITY_MASK|SCHED_ASSIST_UX_MASK);
				ux_state |= ux_orig;
			}
		} else if (ux_value & SA_OPT_SET_PRIORITY) {
			if (ux_orig & SCHED_ASSIST_UX_MASK) {
				/* only change current ux priority */
				ux_state = ux_value & SCHED_ASSIST_UX_PRIORITY_MASK;
				ux_state |= (ux_orig & SCHED_ASSIST_UX_MASK);
			} else {
				/* current isn't ux, don't set ux priority */
				need_update = false;
			}
		} else {
			if ((ux_value & SCHED_ASSIST_UX_MASK) == SA_OPT_CLEAR) {
				/* clear all ux type but animator type */
				ux_state = ux_orig & (SCHED_ASSIST_UX_PRIORITY_MASK|SA_TYPE_ANIMATOR);
			} else {
				/* reset target ux type bit */
				ux_value = ~(ux_value & SCHED_ASSIST_UX_MASK);
				ux_state = ux_orig & (SCHED_ASSIST_UX_PRIORITY_MASK|ux_value);
			}
			/* if ux_state->0 after clear ux bit, and it is inherited, should keep it */
			if (!(ux_state & SCHED_ASSIST_UX_MASK) && (ux_orig & SA_TYPE_INHERIT)) {
				need_update = false;
			}
		}

		if (need_update) {
			oplus_set_ux_state_lock(ux_task, ux_state, -1, true);
		}

		put_task_struct(ux_task);
		ret = ux_state;
	}

	return ret;
}

static long proc_ux_task_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = -1;
	void __user *uarg = (void __user *)arg;

	if (cmd == CMD_UX_READ) {
		int read_ux_param[2];
		pid_t pid, tid;

		if (copy_from_user(read_ux_param, uarg, sizeof(read_ux_param))) {
			return -EFAULT;
		}

		pid = read_ux_param[0];
		tid = read_ux_param[1];
		ret = read_task_ux(pid, tid, true);
	} else if (cmd == CMD_UX_WRITE) {
		int write_ux_param[3];
		pid_t pid, tid;
		int ux_value;

		if (copy_from_user(write_ux_param, uarg, sizeof(write_ux_param))) {
			return -EFAULT;
		}

		pid = write_ux_param[0];
		tid = write_ux_param[1];
		ux_value = write_ux_param[2];
		ret = write_task_ux(pid, tid, ux_value, true);
	}

	return ret;
}

static long proc_ux_task_app_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = -1;
	void __user *uarg = (void __user *)arg;

	if (cmd == CMD_UX_READ) {
		int read_ux_param[2];
		pid_t pid, tid;

		if (copy_from_user(read_ux_param, uarg, sizeof(read_ux_param))) {
			return -EFAULT;
		}

		pid = read_ux_param[0];
		tid = read_ux_param[1];
		ret = read_task_ux(pid, tid, false);
	} else if (cmd == CMD_UX_WRITE) {
		int write_ux_param[3];
		pid_t pid, tid;
		int ux_value;

		if (copy_from_user(write_ux_param, uarg, sizeof(write_ux_param))) {
			return -EFAULT;
		}

		pid = write_ux_param[0];
		tid = write_ux_param[1];
		ux_value = write_ux_param[2];
		ret = write_task_ux(pid, tid, ux_value, false);
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long proc_ux_task_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return proc_ux_task_ioctl(file, cmd, (unsigned long)(compat_ptr(arg)));
}
#endif

static const struct proc_ops proc_debug_enabled_fops = {
	.proc_write		= proc_debug_enabled_write,
	.proc_read		= proc_debug_enabled_read,
	.proc_lseek		= default_llseek,
};

static int im_flag_set_handle(struct task_struct *task, int im_flag)
{
	struct oplus_task_struct *ots = get_oplus_task_struct(task);
#ifdef CONFIG_LOCKING_PROTECT
	unsigned long old_im;
#endif

	if (IS_ERR_OR_NULL(ots))
		return 0;

#ifdef CONFIG_LOCKING_PROTECT
	old_im = ots->im_flag;
#endif
#ifdef CONFIG_OPLUS_CPU_AUDIO_PERF
	oplus_sched_assist_audio_perf_addIm(task, im_flag);
#endif

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_LOADBALANCE)
	/*
	 * Note:
	 * The following operations are order sensitive.
	 */
	if (im_flag < IM_FLAG_CLEAR && im_flag_to_prio(1UL << im_flag) > 0) {
		set_im_flag_with_bit(im_flag, task);
		add_rt_boost_task(task);
	} else {
		remove_rt_boost_task(task);
		set_im_flag_with_bit(im_flag, task);
	}
#else
	set_im_flag_with_bit(im_flag, task);
#endif

	if (test_bit(IM_FLAG_LAUNCHER_NON_UX_RENDER, &ots->im_flag)) {
		int ux_state = oplus_get_ux_state(task);

		oplus_set_ux_state_lock(task, ux_state | SA_TYPE_HEAVY, -1, true);
	}
#ifdef CONFIG_LOCKING_PROTECT
	/* Optimization of ams/wsm lock contention */
	LOCKING_CALL_OP(opt_ss_lock_contention, task, old_im, im_flag);
#endif
	return 0;
}

bool is_kthread(struct task_struct *tsk)
{
	return !!(tsk->flags & PF_KTHREAD);
}

static ssize_t proc_im_flag_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	char buffer[MAX_SET];
	char *str, *token;
	char opt_str[OPT_STR_MAX][8];
	int cnt = 0;
	int pid = 0;
	int im_flag = 0;
	int err = 0;
	static DEFINE_MUTEX(sa_im_mutex);

	memset(buffer, 0, sizeof(buffer));

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	buffer[count] = '\0';
	str = strstrip(buffer);
	while ((token = strsep(&str, " ")) && *token && (cnt < OPT_STR_MAX)) {
		strlcpy(opt_str[cnt], token, sizeof(opt_str[cnt]));
		cnt += 1;
	}

	if (cnt != OPT_STR_MAX) {
		if (cnt == (OPT_STR_MAX - 1) && !strncmp(opt_str[OPT_STR_TYPE], "r", 1)) {
			err = kstrtoint(strstrip(opt_str[OPT_STR_PID]), 10, &pid);
			if (err)
				return err;

			if (pid > 0 && pid <= PID_MAX_DEFAULT)
				global_im_flag_pid = pid;

			return count;
		} else {
			return -EFAULT;
		}
	}

	err = kstrtoint(strstrip(opt_str[OPT_STR_PID]), 10, &pid);
	if (err)
		return err;

	err = kstrtoint(strstrip(opt_str[OPT_STR_VAL]), 10, &im_flag);
	if (err)
		return err;

	mutex_lock(&sa_im_mutex);
	if (!strncmp(opt_str[OPT_STR_TYPE], "p", 1)) {
		struct task_struct *task = NULL;

		if (pid > 0 && pid <= PID_MAX_DEFAULT) {
			rcu_read_lock();
			task = find_task_by_vpid(pid);

			if (task && !is_kthread(task)) {
				get_task_struct(task);
				im_flag_set_handle(task, im_flag);
				put_task_struct(task);
			} else {
				ux_debug("Invalid task pid=%d\n", pid);
			}

			rcu_read_unlock();
		}
	}

	mutex_unlock(&sa_im_mutex);
	return count;
}

static ssize_t proc_im_flag_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	char buffer[256];
	size_t len = 0;
	struct task_struct *task = NULL;

	task = find_task_by_vpid(global_im_flag_pid);
	if (task) {
		get_task_struct(task);
		len = snprintf(buffer, sizeof(buffer), "comm=%s pid=%d tgid=%d im_flag=0x%08lx\n",
			task->comm, task->pid, task->tgid, oplus_get_im_flag(task));
		put_task_struct(task);
	} else
		len = snprintf(buffer, sizeof(buffer), "Can not find task\n");

	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static inline bool can_access_im_flag_app(struct task_struct *task)
{
	return task->tgid == current->tgid;
}

/*
 * this handles "im_flag_app" proc point, only accepts that app change the im flag of its child threads.
 * audio app will use this to change im flag.
 */
static ssize_t proc_im_flag_app_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	char buffer[MAX_SET];
	char *str, *token;
	char opt_str[OPT_STR_MAX][8];
	int cnt = 0;
	int pid = 0;
	int im_flag = 0;
	int err = 0;
	static DEFINE_MUTEX(sa_im_mutex);

	memset(buffer, 0, sizeof(buffer));

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	buffer[count] = '\0';
	str = strstrip(buffer);
	while ((token = strsep(&str, " ")) && *token && (cnt < OPT_STR_MAX)) {
		strlcpy(opt_str[cnt], token, sizeof(opt_str[cnt]));
		cnt += 1;
	}

	if (cnt != OPT_STR_MAX) {
		if (cnt == (OPT_STR_MAX - 1) && !strncmp(opt_str[OPT_STR_TYPE], "r", 1)) {
			err = kstrtoint(strstrip(opt_str[OPT_STR_PID]), 10, &pid);
			if (err)
				return err;

			if (pid > 0 && pid <= PID_MAX_DEFAULT)
				global_im_flag_pid = pid;

			return count;
		} else {
			return -EFAULT;
		}
	}

	err = kstrtoint(strstrip(opt_str[OPT_STR_PID]), 10, &pid);
	if (err)
		return err;

	err = kstrtoint(strstrip(opt_str[OPT_STR_VAL]), 10, &im_flag);
	if (err)
		return err;

	mutex_lock(&sa_im_mutex);
	if (!strncmp(opt_str[OPT_STR_TYPE], "p", 1)) {
		struct task_struct *task = NULL;

		if (pid > 0 && pid <= PID_MAX_DEFAULT) {
			rcu_read_lock();
			task = find_task_by_vpid(pid);
			if (task) {
				get_task_struct(task);
			}
			rcu_read_unlock();

			if (task) {
				if (can_access_im_flag_app(task))
					im_flag_set_handle(task, im_flag);
				put_task_struct(task);
			} else {
				ux_debug("Can not find task with pid=%d", pid);
			}
		}
	}

	mutex_unlock(&sa_im_mutex);
	return count;
}

/*
 * this handles "im_flag_app" proc point, only accepts that app change the im flag of its child threads.
 * audio app will use this to change im flag.
 */
static ssize_t proc_im_flag_app_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	char buffer[256];
	size_t len = 0;
	struct task_struct *task = NULL;

	task = find_task_by_vpid(global_im_flag_pid);
	if (task && can_access_im_flag_app(task)) {
		get_task_struct(task);
		len = snprintf(buffer, sizeof(buffer), "comm=%s pid=%d tgid=%d im_flag=0x%08lx\n",
			task->comm, task->pid, task->tgid, oplus_get_im_flag(task));
		put_task_struct(task);
	} else
		len = snprintf(buffer, sizeof(buffer), "Can not find task\n");

	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t proc_sched_impt_task_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	char temp_buf[32];
	char *temp_str, *token;
	char in_str[2][16];
	int cnt, err, pid;

	static DEFINE_MUTEX(impt_thd_mutex);

	mutex_lock(&impt_thd_mutex);

	memset(temp_buf, 0, sizeof(temp_buf));

	if (count > sizeof(temp_buf) - 1) {
		mutex_unlock(&impt_thd_mutex);
		return -EFAULT;
	}

	if (copy_from_user(temp_buf, buf, count)) {
		mutex_unlock(&impt_thd_mutex);
		return -EFAULT;
	}

	cnt = 0;
	temp_buf[count] = '\0';
	temp_str = strstrip(temp_buf);
	while ((token = strsep(&temp_str, " ")) && *token && (cnt < 2)) {
		strlcpy(in_str[cnt], token, sizeof(in_str[cnt]));
		cnt += 1;
	}

	if (cnt != 2) {
		mutex_unlock(&impt_thd_mutex);
		return -EFAULT;
	}

	err = kstrtoint(strstrip(in_str[1]), 10, &pid);
	if (err) {
		mutex_unlock(&impt_thd_mutex);
		return err;
	}

	if (pid < 0 || pid > PID_MAX_DEFAULT) {
		mutex_unlock(&impt_thd_mutex);
		return -EINVAL;
	}

	/* set top app */
	if (!strncmp(in_str[0], "fg", 2)) {
		save_top_app_tgid = pid;
		top_app_type = 0;
		if (!strncmp(in_str[0], "fgLauncher", 10))
			top_app_type = 1; /* 1 is launcher */
		goto out;
	}

	/* set audio app */
	if (!strncmp(in_str[0], "au", 2)) {
		save_audio_tgid = pid;
		goto out;
	}

out:
	mutex_unlock(&impt_thd_mutex);

	return count;
}

static ssize_t proc_sched_impt_task_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	char buffer[32];
	size_t len = 0;

	len = snprintf(buffer, sizeof(buffer), "top(%d %u) au(%d)\n", save_top_app_tgid, top_app_type, save_audio_tgid);

	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t proc_disable_setting_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	char buffer[8];
	int err, val;

	memset(buffer, 0, sizeof(buffer));

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	buffer[count] = '\0';
	err = kstrtoint(strstrip(buffer), 10, &val);
	if (err)
		return err;

	disable_setting = val;

	return count;
}

static ssize_t proc_disable_setting_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	char buffer[20];
	size_t len = 0;

	len = snprintf(buffer, sizeof(buffer), "%d\n", disable_setting);

	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t proc_hint_message_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	char buffer[8];
	int err, val;

	memset(buffer, 0, sizeof(buffer));

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	buffer[count] = '\0';
	err = kstrtoint(strstrip(buffer), 10, &val);
	if (err)
		return err;

	hint_message = val;

	return count;
}

static ssize_t proc_hint_message_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	char buffer[20];
	size_t len = 0;

	len = snprintf(buffer, sizeof(buffer), "%d\n", hint_message);

	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}
#ifdef CONFIG_OPLUS_FEATURE_TICK_GRAN
static ssize_t proc_retired_instrs_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	char buffer[64];
	size_t len = 0;
	u64 total_instr = 0;
	int cpu;

	for_each_possible_cpu(cpu)
		total_instr += per_cpu(retired_instrs, cpu);

	len = snprintf(buffer, sizeof(buffer), "%llu %llu\n",
						total_instr, total_instr - last_total_instr);

	last_total_instr = total_instr;

	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t proc_ncsw_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	char buffer[128];
	size_t len = 0;
	u64 total_ncsw, total_nvcsw = 0, total_nivcsw = 0;
	int cpu;

	for_each_possible_cpu(cpu) {
		total_nvcsw += per_cpu(nvcsw, cpu);
		total_nivcsw += per_cpu(nivcsw, cpu);
	}
	total_ncsw = total_nvcsw + total_nivcsw;

	len = snprintf(buffer, sizeof(buffer), "ncsw:%llu %llu nvcsw:%llu %llu\n",
						total_ncsw, total_ncsw - last_total_ncsw,
						total_nvcsw, total_nvcsw - last_total_nvcsw);

	last_total_ncsw = total_ncsw;
	last_total_nvcsw = total_nvcsw;

	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}
#endif

static const struct proc_ops proc_sched_assist_enabled_fops = {
	.proc_write		= proc_sched_assist_enabled_write,
	.proc_read		= proc_sched_assist_enabled_read,
	.proc_lseek		= default_llseek,
};

static const struct proc_ops proc_sched_assist_scene_fops = {
	.proc_write		= proc_sched_assist_scene_write,
	.proc_read		= proc_sched_assist_scene_read,
	.proc_lseek		= default_llseek,
};

static const struct proc_ops proc_ux_task_fops = {
	.proc_write		= proc_ux_task_write,
	.proc_read		= proc_ux_task_read,
	.proc_lseek		= default_llseek,
	.proc_ioctl		= proc_ux_task_ioctl,
#ifdef CONFIG_COMPAT
	.proc_compat_ioctl	= proc_ux_task_compat_ioctl,
#endif
};

static const struct proc_ops proc_ux_task_app_fops = {
	.proc_write		= NULL,
	.proc_read		= NULL,
	.proc_lseek		= NULL,
	.proc_ioctl		= proc_ux_task_app_ioctl,
#ifdef CONFIG_COMPAT
	.proc_compat_ioctl	= proc_ux_task_compat_ioctl,
#endif
};

static const struct proc_ops proc_im_flag_fops = {
	.proc_write		= proc_im_flag_write,
	.proc_read		= proc_im_flag_read,
	.proc_lseek		= default_llseek,
};

static const struct proc_ops proc_im_flag_app_fops = {
	.proc_write		= proc_im_flag_app_write,
	.proc_read		= proc_im_flag_app_read,
	.proc_lseek		= default_llseek,
};

static const struct proc_ops proc_sched_impt_task_fops = {
	.proc_write		= proc_sched_impt_task_write,
	.proc_read		= proc_sched_impt_task_read,
	.proc_lseek		= default_llseek,
};

static const struct proc_ops proc_disable_setting_fops = {
	.proc_write		= proc_disable_setting_write,
	.proc_read		= proc_disable_setting_read,
	.proc_lseek		= default_llseek,
};

static const struct proc_ops proc_hint_message_fops = {
	.proc_write		= proc_hint_message_write,
	.proc_read		= proc_hint_message_read,
	.proc_lseek		= default_llseek,
};
#ifdef CONFIG_OPLUS_FEATURE_TICK_GRAN
static const struct proc_ops proc_retired_instrs_fops = {
	.proc_read		= proc_retired_instrs_read,
};

static const struct proc_ops proc_ncsw_fops = {
	.proc_read		= proc_ncsw_read,
};
#endif

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_LOADBALANCE)
extern void oplus_lb_proc_init(struct proc_dir_entry *pde);
extern void oplus_lb_proc_deinit(struct proc_dir_entry *pde);
#endif

int oplus_sched_assist_proc_init(void)
{
	struct proc_dir_entry *proc_node;
	struct device_node *device_node = NULL;

	d_oplus_scheduler = proc_mkdir(OPLUS_SCHEDULER_PROC_DIR, NULL);
	if (!d_oplus_scheduler) {
		ux_err("failed to create proc dir oplus_scheduler\n");
		goto err_creat_d_oplus_scheduler;
	}

	d_sched_assist = proc_mkdir(OPLUS_SCHEDASSIST_PROC_DIR, d_oplus_scheduler);
	if (!d_sched_assist) {
		ux_err("failed to create proc dir sched_assist\n");
		goto err_creat_d_sched_assist;
	}

	proc_node = proc_create("debug_enabled", 0666, d_sched_assist, &proc_debug_enabled_fops);
	if (!proc_node) {
		ux_err("failed to create proc node debug_enabled\n");
		goto err_creat_debug_enabled;
	}

	proc_node = proc_create("sched_assist_enabled", 0666, d_sched_assist, &proc_sched_assist_enabled_fops);
	if (!proc_node) {
		ux_err("failed to create proc node sched_assist_enabled\n");
		goto err_creat_sched_assist_enabled;
	}

	proc_node = proc_create("sched_assist_scene", 0666, d_sched_assist, &proc_sched_assist_scene_fops);
	if (!proc_node) {
		ux_err("failed to create proc node sched_assist_scene\n");
		goto err_creat_sched_assist_scene;
	}

	proc_node = proc_create("ux_task", 0666, d_sched_assist, &proc_ux_task_fops);
	if (!proc_node) {
		ux_err("failed to create proc node ux_task\n");
		goto err_creat_ux_task;
	}

	proc_node = proc_create("ux_task_app", 0666, d_sched_assist, &proc_ux_task_app_fops);
	if (!proc_node) {
		ux_err("failed to create proc node ux_task_app\n");
		goto err_creat_ux_task;
	}

	proc_node = proc_create("im_flag", 0666, d_sched_assist, &proc_im_flag_fops);
	if (!proc_node) {
		ux_err("failed to create proc node im_flag\n");
		remove_proc_entry("im_flag", d_sched_assist);
	}

	proc_node = proc_create("im_flag_app", 0666, d_sched_assist, &proc_im_flag_app_fops);
	if (!proc_node) {
		ux_err("failed to create proc node im_flag_app\n");
		remove_proc_entry("im_flag_app", d_sched_assist);
	}

	proc_node = proc_create("sched_impt_task", 0666, d_sched_assist, &proc_sched_impt_task_fops);
	if (!proc_node) {
		ux_err("failed to create proc node sched_impt_task\n");
		remove_proc_entry("sched_impt_task", d_sched_assist);
	}

	proc_node = proc_create("hint_message", 0666, d_sched_assist, &proc_hint_message_fops);
	if (!proc_node) {
		ux_err("failed to create proc node hint_message\n");
		remove_proc_entry("hint_message", d_sched_assist);
	}

	proc_node = proc_create("disable_setting", 0666, d_sched_assist, &proc_disable_setting_fops);
	if (!proc_node) {
		ux_err("failed to create proc node disable_setting\n");
		remove_proc_entry("disable_setting", d_sched_assist);
	}

#ifdef CONFIG_OPLUS_FEATURE_TICK_GRAN
	proc_node = proc_create("retired_instrs", 0666, d_sched_assist, &proc_retired_instrs_fops);
	if (!proc_node) {
		ux_err("failed to create proc node retired_instrs\n");
		remove_proc_entry("retired_instrs", d_sched_assist);
	}

	proc_node = proc_create("nr_switches", 0666, d_sched_assist, &proc_ncsw_fops);
	if (!proc_node) {
		ux_err("failed to create proc node ncsw\n");
		remove_proc_entry("nr_switches", d_sched_assist);
	}
#endif

	device_node = of_find_compatible_node(NULL, NULL, "oplus,sched_assit");
	if (device_node)
		disable_setting = 0;

#ifdef CONFIG_OPLUS_CPU_AUDIO_PERF
	oplus_sched_assist_audio_proc_init(d_sched_assist);
#endif

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_LOADBALANCE)
	oplus_lb_proc_init(d_sched_assist);
#endif

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_PIPELINE)
	oplus_pipeline_init(d_sched_assist);
#endif

	return 0;

err_creat_ux_task:
	remove_proc_entry("sched_assist_scene", d_sched_assist);

err_creat_sched_assist_scene:
	remove_proc_entry("sched_assist_enabled", d_sched_assist);

err_creat_sched_assist_enabled:
	remove_proc_entry("debug_enabled", d_sched_assist);

err_creat_debug_enabled:
	remove_proc_entry(OPLUS_SCHEDASSIST_PROC_DIR, d_oplus_scheduler);

err_creat_d_sched_assist:
	remove_proc_entry(OPLUS_SCHEDULER_PROC_DIR, NULL);

err_creat_d_oplus_scheduler:
	return -ENOENT;
}

void oplus_sched_assist_proc_deinit(void)
{
#if IS_ENABLED(CONFIG_OPLUS_FEATURE_LOADBALANCE)
	oplus_lb_proc_deinit(d_sched_assist);
#endif

	remove_proc_entry("ux_task", d_sched_assist);
	remove_proc_entry("sched_assist_scene", d_sched_assist);
	remove_proc_entry("sched_assist_enabled", d_sched_assist);
	remove_proc_entry(OPLUS_SCHEDASSIST_PROC_DIR, d_oplus_scheduler);
	remove_proc_entry(OPLUS_SCHEDULER_PROC_DIR, NULL);
}

