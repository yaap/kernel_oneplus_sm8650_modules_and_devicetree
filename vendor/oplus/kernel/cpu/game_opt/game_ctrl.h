// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2022 Oplus. All rights reserved.
 */

#ifndef __GAME_CTRL_H__
#define __GAME_CTRL_H__

#include <linux/sched.h>
#include <linux/sched/cputime.h>
#include <kernel/sched/sched.h>

#define MAX_TID_COUNT 256
#define MAX_TASK_NR 15
#define RESULT_PAGE_SIZE 1024

extern struct proc_dir_entry *game_opt_dir;
extern struct proc_dir_entry *early_detect_dir;

extern atomic_t have_valid_game_pid;
extern atomic_t have_valid_render_pid;

extern int g_debug_enable;
extern inline void systrace_c_printk(const char *msg, unsigned long val);
extern inline void systrace_c_signed_printk(const char *msg, long val);

int cpu_load_init(void);
int cpufreq_limits_init(void);
int task_util_init(void);
int rt_info_init(void);
int fake_cpufreq_init(void);
int early_detect_init(void);
int debug_init(void);

bool get_task_name(pid_t pid, struct task_struct *in_task, char *name);
void ui_assist_threads_wake_stat(struct task_struct *task);

/*----------------------------- early detect start -----------------------------*/
enum ED_BOOST_TYPE {
	ED_BOOST_NONE = 0,
	ED_BOOST_EDB = (1 << 0),
	ED_BOOST_RML = (1 << 1), /* frame drop Release Max frequency Limits */
	ED_BOOST_FST = (1 << 2),
	ED_BOOST_FLT = (1 << 3)
};

void ed_freq_boost_request(unsigned int boost_type);
void ed_render_wakeup_times_stat(struct task_struct *task);
void ed_set_render_task(struct task_struct *render_task);
/*----------------------------- early detect end -----------------------------*/

#endif /*__GAME_CTRL_H__*/
