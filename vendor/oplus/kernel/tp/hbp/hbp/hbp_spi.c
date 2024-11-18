
#include "utils/debug.h"
#include "hbp_spi.h"
#include <linux/sched/debug.h>
#include "hbp_core.h"
#include <linux/delay.h>

extern struct task_struct *suspend_task;

#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
#else
#include <linux/platform_data/spi-mt65xx.h>

const struct mtk_chip_config st_spi_ctrdata = {
	.sample_sel = 0,
	.cs_setuptime = 5000,
	.cs_holdtime = 3000,
	.cs_idletime = 0,
	.tick_delay = 0,
};
#endif
#endif

static inline int __hbp_spi_alloc_mem(struct spi_transfer **spi_xfer,
				      size_t xfer_len,
				      uint8_t **tx,
				      size_t tx_size,
				      uint8_t **rx,
				      size_t rx_size)
{
	int ret = 0;
	static struct spi_transfer *xfer = NULL;
	static uint32_t xfer_count = 0;
	static uint8_t *tx_buf = NULL;
	static size_t tx_count = 0;
	static uint8_t *rx_buf = NULL;
	static size_t rx_count = 0;

	if (xfer_len > xfer_count) {
		kfree(xfer);
		xfer = kcalloc(xfer_len, sizeof(struct spi_transfer), GFP_KERNEL);
		if (!xfer) {
			xfer_count = 0;
			ret = -ENOMEM;
			hbp_err("Failed to calloc memory\n");
			goto err_exit;
		} else {
			xfer_count = xfer_len;
		}
	} else {
		memset(xfer, 0, xfer_len*sizeof(struct spi_transfer));
	}

	if (tx_size > tx_count) {
		kfree(tx_buf);
		tx_buf = (uint8_t *)kmalloc(tx_size, GFP_KERNEL);
		if (!tx_buf) {
			tx_count = 0;
			ret = -ENOMEM;
			hbp_err("Failed to malloc tx memory\n");
			goto err_exit;
		}
		tx_count = tx_size;
	}

	if (tx_size) {
		memset(tx_buf, 0xFF, tx_size);
	}

	if (rx_size > rx_count) {
		kfree(rx_buf);
		rx_buf = (uint8_t *)kmalloc(rx_size, GFP_KERNEL);
		if (!rx_buf) {
			rx_count = 0;
			ret = -ENOMEM;
			hbp_err("Failed to malloc rx memory\n");
			goto err_exit;
		}
		rx_count = rx_size;
	}
	if (rx_size) {
		memset(rx_buf, 0xFF, rx_size);
	}

	*spi_xfer = xfer;
	if (tx_size) {
		*tx = tx_buf;
	}

	if (rx_size) {
		*rx = rx_buf;
	}

err_exit:
	return ret;
}

static inline int __hbp_spi_read_block(struct spi_device *spi_dev,
				       uint8_t *addr,
				       uint32_t addr_len,
				       uint8_t *rbuf,
				       size_t len,
				       struct spi_param *param)
{
	int ret = 0;
	struct spi_message msg;
	struct spi_transfer *xfer = NULL;
	uint8_t *tx_buf = NULL;
	uint8_t *rx_buf = NULL;
	size_t tx_size = addr_len;
	size_t rx_size = len;
	int mode;

	spi_message_init(&msg);

	if (addr) {
		tx_size = addr_len;
		ret = __hbp_spi_alloc_mem(&xfer,
					  2,
					  &tx_buf,
					  tx_size + rx_size,
					  &rx_buf,
					  rx_size);
	} else {
		ret = __hbp_spi_alloc_mem(&xfer,
					  1,
					  &tx_buf,
					  rx_size,
					  &rx_buf,
					  rx_size);
	}

	if (ret < 0) {
		hbp_err("Failed to alloc memory\n");
		goto alloc_err;
	}

	if (addr) {
		memcpy(tx_buf, addr, addr_len);
		xfer[0].len = tx_size;
		xfer[0].tx_buf = &tx_buf[0];
		xfer[0].cs_change = 0;
		if (param->block_delay_us) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0))
			xfer[0].delay_usecs = param->block_delay_us;
#else
			xfer[0].delay.value = param->block_delay_us;
			xfer[0].delay.unit = SPI_DELAY_UNIT_USECS;
#endif
		}
		spi_message_add_tail(&xfer[0], &msg);

		xfer[1].len = rx_size;
		xfer[1].tx_buf = &tx_buf[tx_size];
		xfer[1].rx_buf = rx_buf;
		xfer[1].cs_change = 0;

		if (param->block_delay_us) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0))
			xfer[1].delay_usecs = param->block_delay_us;
#else
			xfer[1].delay.value = param->block_delay_us;
			xfer[1].delay.unit = SPI_DELAY_UNIT_USECS;
#endif
		}
		spi_message_add_tail(&xfer[1], &msg);
	} else {
		xfer[0].len = rx_size;
		xfer[0].tx_buf = tx_buf;
		xfer[0].rx_buf = rx_buf;
		xfer[0].cs_change = 0;
		if (param->block_delay_us) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0))
			xfer[0].delay_usecs = param->block_delay_us;
#else
			xfer[0].delay.value = param->block_delay_us;
			xfer[0].delay.unit = SPI_DELAY_UNIT_USECS;
#endif
		}
		spi_message_add_tail(&xfer[0], &msg);
	}

	mode = spi_dev->mode;
	spi_dev->mode = param->mode;

	ret = spi_sync(spi_dev, &msg);
	if (ret) {
		hbp_err("Failed to complete SPI transfer, error:%d\n", ret);
		hbp_exception_report(EXCEP_BUS, "spi_read_block fail", sizeof("spi_read_block fail"));
		goto sync_err;
	}

	memcpy(rbuf, rx_buf, len);
	ret = len;

sync_err:
	spi_dev->mode = mode;
alloc_err:
	return ret;
}

static inline int __hbp_spi_write_block(struct spi_device *spi_dev,
					uint8_t *addr,
					size_t addr_len,
					uint8_t *wbuf,
					size_t len,
					struct spi_param *param)
{
	int ret = 0;
	struct spi_message msg;
	struct spi_transfer *xfer = NULL;
	uint8_t *tx_buf = NULL;
	size_t tx_size = addr_len + len;
	int mode;

	spi_message_init(&msg);

	ret = __hbp_spi_alloc_mem(&xfer,
				  1,
				  &tx_buf,
				  tx_size,
				  NULL,
				  0);

	if (ret < 0) {
		hbp_err("Failed to alloc memory\n");
		goto alloc_err;
	}

	if (addr) {
		memcpy(tx_buf, addr, addr_len);
	}

	memcpy(tx_buf + addr_len, wbuf, len);
	xfer[0].len = tx_size;
	xfer[0].tx_buf = tx_buf;
	xfer[0].cs_change = 0;
	if (param->block_delay_us) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0))
		xfer[0].delay_usecs = param->block_delay_us;
#else
		xfer[0].delay.value = param->block_delay_us;
		xfer[0].delay.unit = SPI_DELAY_UNIT_USECS;
#endif
	}
	spi_message_add_tail(&xfer[0], &msg);

	mode = spi_dev->mode;
	spi_dev->mode = param->mode;

	ret = spi_sync(spi_dev, &msg);
	if (ret) {
		hbp_err("Failed to complete SPI transfer, error:%d\n", ret);
		hbp_exception_report(EXCEP_BUS, "spi_write_block fail", sizeof("spi_write_block fail"));
		goto sync_err;
	}

	ret = len;

sync_err:
	spi_dev->mode = mode;

alloc_err:
	return ret;
}

static int __hbp_spi_sync(struct spi_device *spi, u8 *tx, u8 *rx, u32 len, struct spi_param *param)
{
	int ret = 0;
	uint8_t *tx_buf = NULL;
	uint8_t *rx_buf = NULL;
	struct spi_message msg;
	struct spi_transfer *xfer;

	spi_message_init(&msg);

	ret = __hbp_spi_alloc_mem(&xfer,
				  1,
				  &tx_buf,
				  len,
				  &rx_buf,
				  len);

	if (tx) {
		memcpy(tx_buf, tx, len);
	}

	xfer[0].len = len;
	xfer[0].tx_buf = &tx_buf[0];
	xfer[0].rx_buf = &rx_buf[0];
	xfer[0].cs_change = 0;

	spi_message_add_tail(&xfer[0], &msg);

	ret = spi_sync(spi, &msg);
	if (ret) {
		hbp_err("spi_sync fail,ret:%d", ret);
		hbp_exception_report(EXCEP_BUS, "spi_sync fail", sizeof("spi_sync fail"));
		return ret;
	}

	memcpy(rx, rx_buf, len);

	return ret;
}

static inline bool hbp_spi_bus_ready(struct spi_device *spi_dev)
{
	struct spi_master *master = spi_dev->master;
	struct device *ctrl_dev;
	int retry = 100;
	static int error_cnt = 0;

	if (!master) {
		return true;
	}

	ctrl_dev = master->dev.parent;
	while (--retry) {
		if (!ctrl_dev->power.is_suspended) {
			return false;
		}
		usleep_range(5000, 5000);
	}

	if (suspend_task && error_cnt < 5) {
		hbp_err("hbp spi bus ready time out: %d", error_cnt + 1);
		++error_cnt;
		sched_show_task(suspend_task);
	}
	hbp_err("bus spi in sleep after usleep_range 500 ms, is_suspended %d\n", ctrl_dev->power.is_suspended);
	hbp_exception_report(EXCEP_BUS_READY, "hbp_spi_bus_ready timeout", sizeof("hbp_spi_bus_ready timeout"));
	return true;
}

static int hbp_spi_sync(void *ops, uint8_t *tx, uint8_t *rx, size_t len)
{
	int ret = 0;
	struct spi_bus *bus;

	bus = container_of(ops, struct spi_bus, spi_ops);
	if (IS_ERR_OR_NULL(bus)) {
		hbp_err("fatal: invalid bus\n");
		return -ENODEV;
	}

	if (hbp_spi_bus_ready(bus->spi_dev)) {
		return -EBUSY;
	}

	mutex_lock(&bus->mtx);
	ret = __hbp_spi_sync(bus->spi_dev, tx, rx, len, &bus->param);
	mutex_unlock(&bus->mtx);

	return ret;
}

static int hbp_spi_write_block(void *ops, uint8_t *wbuf, size_t len)
{
	int ret = 0;
	struct spi_bus *bus;

	bus = container_of(ops, struct spi_bus, spi_ops);
	if (IS_ERR_OR_NULL(bus)) {
		hbp_err("fatal: invalid bus\n");
		return -ENODEV;
	}

	if (hbp_spi_bus_ready(bus->spi_dev)) {
		return -EBUSY;
	}

	mutex_lock(&bus->mtx);
	ret = __hbp_spi_write_block(bus->spi_dev, NULL, 0, wbuf, len, &bus->param);
	mutex_unlock(&bus->mtx);

	return ret;
}

static int hbp_spi_read_block(void *ops, uint8_t *rbuf, size_t len)
{
	int ret = 0;
	struct spi_bus *bus;

	bus = container_of(ops, struct spi_bus, spi_ops);
	if (IS_ERR_OR_NULL(bus)) {
		hbp_err("fatal: invalid bus\n");
		return -ENODEV;
	}

	if (hbp_spi_bus_ready(bus->spi_dev)) {
		return -EBUSY;
	}

	mutex_lock(&bus->mtx);
	ret = __hbp_spi_read_block(bus->spi_dev, NULL, 0, rbuf, len, &bus->param);
	mutex_unlock(&bus->mtx);

	return	ret;
}

static int hbp_spi_dt_parse(struct spi_bus *bus, struct spi_device *spi_dev)
{
	struct device_node *np = spi_dev->dev.of_node;
	int mode = 0;
	int ret = 0;

	ret = of_property_read_u32(np, "bus,spi-mode", &mode);
	if (ret < 0) {
		bus->param.mode = SPI_MODE_0;
	} else {
		switch (mode) {
		case 0:
			bus->param.mode = SPI_MODE_0;
			break;
		case 1:
			bus->param.mode = SPI_MODE_1;
			break;
		case 2:
			bus->param.mode = SPI_MODE_2;
			break;
		case 3:
			bus->param.mode = SPI_MODE_3;
			break;
		default:
			break;
		}
	}

	bus->name = np->full_name;
	hbp_info("name:%s, mode %d\n",
		 bus->name,
		 bus->param.mode);

	ret = spi_setup(spi_dev);
	if (ret < 0) {
		hbp_err("Failed to set spi\n");
		return ret;
	}

	return 0;
}

static void hbp_spi_shutdown(void *spi_ops)
{
	/*
		struct spi_bus *bus;
		struct hbp_bus_io *io;
		struct spi_device *spi_dev;

		container_of(bus, spi_ops, )
		io = (struct hbp_bus_io*)bus;
		if (IS_ERR_OR_NULL(io)) {
			hbp_err("Fatal:invalid bus\n");
			return;
		}

		spi_bus = (struct spi_bus_platform *)io->parent;
	 	spi_dev = spi_bus->spi_dev;

		spi_dev->mode |= SPI_CS_HIGH;
		spi_setup(spi_dev);
		*/
}

static int hbp_spi_setup(void *ops, uint8_t mode, uint8_t bits_per_word, int speed)
{
	int ret = 0;
	struct spi_bus *bus;

	bus = container_of(ops, struct spi_bus, spi_ops);
	if (IS_ERR_OR_NULL(bus)) {
		hbp_err("fatal: invalid bus\n");
		return -ENODEV;
	}

	bus->spi_dev->mode = mode;
	bus->spi_dev->bits_per_word = bits_per_word;
	bus->spi_dev->max_speed_hz = speed;

	ret = spi_setup(bus->spi_dev);
	if (ret) {
		hbp_err("failed to set spi\n");
		return -ENODEV;
	}

	return 0;
}

static int hbp_spi_probe(struct spi_device *spi_dev)
{
	int ret = 0;
	struct platform_device *spi_platform;
	struct spi_bus *bus;

	hbp_info("enter.\n");

	bus = kzalloc(sizeof(struct spi_bus), GFP_KERNEL);
	if (!bus) {
		hbp_err("Failed to alloc memory");
		return -ENOMEM;
	}

	ret = hbp_spi_dt_parse(bus, spi_dev);
	if (ret < 0) {
		hbp_err("Failed to parse spi dt\n");
		goto err_exit;
	}

	spi_platform = platform_device_alloc(bus->name, 0);
	if (!spi_platform) {
		hbp_err("Failed to alloc platform device\n");
		ret = -ENODEV;
		goto err_exit;
	}

	bus->spi_dev = spi_dev;

	mutex_init(&bus->mtx);
	bus->spi_ops.read_block = hbp_spi_read_block;
	bus->spi_ops.write_block = hbp_spi_write_block;
	bus->spi_ops.spi_sync = hbp_spi_sync;
	bus->spi_ops.shutdown = hbp_spi_shutdown;
	bus->spi_ops.spi_setup = hbp_spi_setup;

	spi_set_drvdata(spi_dev, bus);

	spi_platform->dev.parent = &spi_dev->dev;
	spi_platform->dev.platform_data = bus;
	spi_platform->id = PLATFORM_DEVID_NONE;
#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
#else
	spi_dev->controller_data = (void *)&st_spi_ctrdata;
#endif
#endif

	ret = platform_device_add(spi_platform);
	if (ret < 0) {
		hbp_err("Failed to add platform device\n");
		goto err_exit;
	}

	hbp_info("exit.\n");

	return 0;

err_exit:
	kfree(bus);
	return ret;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
#define REMOVE_RESULT_OK
static void hbp_spi_remove(struct spi_device *spi)
#else
static int hbp_spi_remove(struct spi_device *spi)
#define REMOVE_RESULT_OK (0)
#endif
{
	//struct spi_bus_platform *spi_bus;

	return REMOVE_RESULT_OK;
}

static int hbp_spi_suspend(struct device *dev)
{
	struct spi_bus *bus = dev_get_drvdata(dev);

	hbp_info("enter.\n");

	if (!bus) {
		hbp_err("bus is null.\n");
		return 0;
	}
	bus->bus_ready = false;
	return 0;
}

static int hbp_spi_resume(struct device *dev)
{
	struct spi_bus *bus = dev_get_drvdata(dev);

	hbp_info("enter.\n");

	if (!bus) {
		hbp_err("bus is null.\n");
		return 0;
	}
	bus->bus_ready = true;
	return 0;
}

static struct of_device_id hbp_of_match_table[] = {
	{.compatible = "oplus,hbp_spi_bus"},
	{},
};

static const struct dev_pm_ops hbp_spi_pm_ops = {
	.suspend = hbp_spi_suspend,
	.resume = hbp_spi_resume,
};

static struct spi_driver hbp_spi_driver = {
	.driver = {
		.name = HBP_SPI_MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = hbp_of_match_table,
		.pm = &hbp_spi_pm_ops,
	},
	.probe = hbp_spi_probe,
	.remove = hbp_spi_remove,
};

int hw_interface_init(void)
{
	return spi_register_driver(&hbp_spi_driver);
}


