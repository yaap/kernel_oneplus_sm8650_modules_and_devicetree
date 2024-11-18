// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023-2023 Oplus. All rights reserved.
 */
#define pr_fmt(fmt) "[1WIRE_PROTOCOL]([%s][%d]): " fmt, __func__, __LINE__

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/gpio/driver.h>
#include <linux/gpio/consumer.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <oplus_chg_ic.h>

#include "1wire_protocol.h"

#define ONE_WIRE_CONFIG_OUT		writel_relaxed(g_onewire_data->onewire_gpio_cfg_out_val, g_onewire_data->gpio_cfg_out_reg)
#define ONE_WIRE_CONFIG_IN		writel_relaxed(g_onewire_data->onewire_gpio_cfg_in_val, g_onewire_data->gpio_cfg_in_reg)
#define ONE_WIRE_OUT_HIGH		writel_relaxed(g_onewire_data->onewire_gpio_level_high_val, g_onewire_data->gpio_out_high_reg)
#define ONE_WIRE_OUT_LOW		writel_relaxed(g_onewire_data->onewire_gpio_level_low_val, g_onewire_data->gpio_out_low_reg)

static struct onewire_gpio_data *g_onewire_data;

int write_get_pin(void);
void delay_us(unsigned int delay_us);
void delay_ms(unsigned int delay_ms);

/* Basic 1-Wire functions */
int ow_reset(void);
void write_byte(unsigned char byte_value);
void write_bit(unsigned char bit_value);
unsigned char read_bit(void);
unsigned char read_byte(void);

/*******************************************************************************
 get 1-wire GPIO input
*******************************************************************************/
int write_get_pin(void)
{
	int ret = 0;
	ret = gpio_get_value(g_onewire_data->ow_gpio);
	return ret;
}
/*****************************************************************************
delay us subroutine
*****************************************************************************/
void delay_us(unsigned int delay_us)    /* 1US */
{
	udelay(delay_us);
}

void delay_ns(unsigned int delay_ns)
{
	ndelay(delay_ns);
}

/*****************************************************************************
delay ms subroutine
*****************************************************************************/
void delay_ms(unsigned int delay_ms)    /* 1US */
{
	mdelay(delay_ms);
}

/*****************************************************************************
Basic 1-Wire functions
Reset all of the devices on the 1-Wire Net and return the result.
Returns: TRUE(1):  presense pulse(s) detected, device(s) reset
      FALSE(0): no presense pulses detected
*****************************************************************************/
int ow_reset(void)
{
	unsigned int presence = 0;
	unsigned int value = 0;
	unsigned int gpio_value = 0;
	unsigned long flags;

	raw_spin_lock_irqsave(&g_onewire_data->lock, flags);
	ONE_WIRE_CONFIG_OUT;
	ONE_WIRE_OUT_LOW;
	delay_us(54);	/* wait 54 us */
	ONE_WIRE_CONFIG_IN;
	delay_us(9);	/* wait 9 us for presence */
	value = readl_relaxed(g_onewire_data->gpio_in_reg);
	presence = !(value >> g_onewire_data->gpio_addr_offset & 0x1);
	chg_info(" %s ++++gpio_value 0x%x value 0x%x presence 0x%x", __func__, gpio_value, value, presence);
	delay_us(50);	/* wait 50 us */
	ONE_WIRE_OUT_HIGH;
	ONE_WIRE_CONFIG_IN;
	raw_spin_unlock_irqrestore(&g_onewire_data->lock, flags);

	return(presence); /* presence signal returned */
}

/* Send 1 bit of communication to the 1-Wire Net.
 The parameter 'sendbit' least significant bit is used.
 'bitvalue' - 1 bit to send (least significant byte)
 */
void write_bit(unsigned char bitval)
{
	ONE_WIRE_OUT_LOW;
	delay_us(1);			/* keeping logic low for 1 us */
	if (bitval != 0)
		ONE_WIRE_OUT_HIGH;	/* ONE_WIRE_OUT_HIGH; set 1-wire to logic high if bitval='1' */
	delay_us(10);			/*  waiting for 10us */
	ONE_WIRE_OUT_HIGH;
	delay_us(5);			/*  waiting for 5us to recover to logic high */
}

/* Send 1 bit of read communication to the 1-Wire Net and and return the
 result 1 bit read from the 1-Wire Net.
 Returns:  1 bits read from 1-Wire Net
 */
unsigned char read_bit(void)
{
	unsigned int vamm;
	unsigned int value;

	ONE_WIRE_CONFIG_OUT;
	ONE_WIRE_OUT_LOW;
	ONE_WIRE_OUT_LOW;
	ONE_WIRE_CONFIG_IN;
	delay_ns(500);	/* wait 500 ns */
	value = readl_relaxed(g_onewire_data->gpio_in_reg);
	vamm = value >> g_onewire_data->gpio_addr_offset & 0x1;
	delay_us(5);	/* waiting for 5us Keep GPIO at the input state */
	ONE_WIRE_OUT_HIGH;
	ONE_WIRE_CONFIG_OUT;

	delay_us(6);	/* waiting for 6us Keep GPIO at the output state */
	return(vamm);	/*  return value of 1-wire dat pin */
}


/* Send 8 bits of communication to the 1-Wire Net and verify that the
 8 bits read from the 1-Wire Net is the same (write operation).
 The parameter 'sendbyte' least significant 8 bits are used.
 'val' - 8 bits to send (least significant byte)
 Returns:  TRUE: bytes written and echo was the same
           FALSE: echo was not the same
 */
void write_byte(unsigned char val)
{
	unsigned char i;
	unsigned char temp;
	unsigned long flags;

	raw_spin_lock_irqsave(&g_onewire_data->lock, flags);
	ONE_WIRE_CONFIG_OUT;
	for (i = 0; i < 8; i++) {	/*  writes byte, one bit at a time */
		temp = val >> i;		/*  shifts val right */
		temp &= 0x01;			/*  copy that bit to temp */
		write_bit(temp);		/*  write bit in temp into */
	}
	raw_spin_unlock_irqrestore(&g_onewire_data->lock, flags);
}

/* Send 8 bits of read communication to the 1-Wire Net and and return the
 result 8 bits read from the 1-Wire Net.
 Returns:  8 bits read from 1-Wire Net
 */
unsigned char read_byte(void)
{
	unsigned char i;
	unsigned char value = 0;
	unsigned long flags;

	raw_spin_lock_irqsave(&g_onewire_data->lock, flags);
	for (i = 0; i < 8; i++)
		if (read_bit()) value |= 0x01 << i;	/* reads byte in, one byte at a time and then shifts it left */

	raw_spin_unlock_irqrestore(&g_onewire_data->lock, flags);

	return(value);
}

int onewire_init(struct onewire_gpio_data *onewire_data)
{
	chg_info("%s entry", __func__);
	g_onewire_data = (struct onewire_gpio_data *)kmalloc(sizeof(struct onewire_gpio_data), GFP_KERNEL);
	if (!g_onewire_data) {
		chg_err("Failed to allocate memory\n");
		return -ENOMEM;
	}

	if (onewire_data == NULL) {
		kfree(g_onewire_data);
		chg_info("%s onewire_data is null return", __func__);
		return -1;
	}
	raw_spin_lock_init(&g_onewire_data->lock);

	g_onewire_data->gpio_cfg_out_reg = onewire_data->gpio_cfg_out_reg;
	g_onewire_data->gpio_cfg_in_reg = onewire_data->gpio_cfg_in_reg;
	g_onewire_data->gpio_out_high_reg = onewire_data->gpio_out_high_reg;
	g_onewire_data->gpio_out_low_reg = onewire_data->gpio_out_low_reg;
	g_onewire_data->gpio_in_reg = onewire_data->gpio_in_reg;
	g_onewire_data->gpio_addr_offset = onewire_data->gpio_addr_offset;
	g_onewire_data->onewire_gpio_cfg_out_val = onewire_data->onewire_gpio_cfg_out_val;
	g_onewire_data->onewire_gpio_cfg_in_val = onewire_data->onewire_gpio_cfg_in_val;
	g_onewire_data->onewire_gpio_level_high_val = onewire_data->onewire_gpio_level_high_val;
	g_onewire_data->onewire_gpio_level_low_val = onewire_data->onewire_gpio_level_low_val;
	ONE_WIRE_CONFIG_OUT;
	ONE_WIRE_OUT_HIGH;
	return 0;
}

void onewire_set_gpio_config_in(void)
{
	ONE_WIRE_CONFIG_IN;
	chg_info("%s set gpio in", __func__);
}

void onewire_set_gpio_config_out(void)
{
	ONE_WIRE_CONFIG_OUT;
	chg_info("%s set gpio out", __func__);
}
