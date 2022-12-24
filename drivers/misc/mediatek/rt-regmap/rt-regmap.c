/*
 *  Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/semaphore.h>
#include <linux/alarmtimer.h>
#include <linux/workqueue.h>

#include <mt-plat/rt-regmap.h>
#define RT_REGMAP_VERSION	"1.1.14_G"

struct rt_regmap_ops {
	int (*regmap_block_write)(struct rt_regmap_device *rd, u32 reg,
			 int bytes, const void *data);
	int (*regmap_block_read)(struct rt_regmap_device *rd, u32 reg,
			int bytes, void *dest);
};

enum {
	RT_DBG_REG,
	RT_DBG_DATA,
	RT_DBG_REGS,
	RT_DBG_SYNC,
	RT_DBG_ERROR,
	RT_DBG_NAME,
	RT_DBG_BLOCK,
	RT_DBG_SIZE,
	RT_DBG_SLAVE_ADDR,
	RT_DBG_SUPPORT_MODE,
	RT_DBG_IO_LOG,
	RT_DBG_CACHE_MODE,
	RT_DBG_REG_SIZE,
	RT_DBG_WATCHDOG,
	RT_DBG_MAX,
};

struct reg_index_offset {
	int index;
	int offset;
};

/* rt_regmap_device
 *
 * Richtek regmap device. One for each rt_regmap.
 *
 */
struct rt_regmap_device {
	struct rt_regmap_properties props;
	struct rt_regmap_fops *rops;
	struct rt_regmap_ops regmap_ops;
	struct alarm watchdog_alarm;
	struct delayed_work watchdog_work;
	struct device dev;
	void *client;
	struct semaphore semaphore;
	struct semaphore write_mode_lock;
	struct delayed_work rt_work;
	unsigned char *cache_flag;
	unsigned char part_size_limit;
	unsigned char *alloc_data;
	unsigned char **cache_data;
	unsigned char *cached;
	char *err_msg;
	int slv_addr;

	int (*rt_block_write[4])(struct rt_regmap_device *rd,
			const struct rt_register *rm, int size,
			const struct reg_index_offset *rio,
			unsigned char *wdata, int *count, int cache_idx);
	unsigned char cache_inited:1;
	unsigned char error_occurred:1;
	unsigned char pending_event:1;
};


static struct reg_index_offset find_register_index(
		const struct rt_regmap_device *rd, u32 reg)
{
	const rt_register_map_t *rm = rd->props.rm;
	int register_num = rd->props.register_num;
	struct reg_index_offset rio = {0, 0};
	int index = 0, i = 0, unit = RT_1BYTE_MODE;

	for (index = 0; index < register_num; index++) {
		if (reg == rm[index]->addr) {
			rio.index = index;
			rio.offset = 0;
			break;
		}
		if (reg > rm[index]->addr) {
			if ((reg - rm[index]->addr) < rm[index]->size) {
				rio.index = index;
				while (&rd->props.group[i] != NULL) {
					if (reg >= rd->props.group[i].start
					&& reg <= rd->props.group[i].end) {
						unit =
							rd->props.group[i].mode;
						break;
					}
					i++;
					unit = RT_1BYTE_MODE;
				}
				rio.offset =
					(reg-rm[index]->addr)*unit;
			} else
				rio.offset = rio.index = -1;
		}
	}
	return rio;
}

static int rt_chip_block_write(struct rt_regmap_device *rd, u32 reg,
				int bytes, const void *src);

/* rt_regmap_cache_sync - sync all cache data to real chip*/
void rt_regmap_cache_sync(struct rt_regmap_device *rd)
{
	int i, rc, num;
	const rt_register_map_t *rm = rd->props.rm;

	down(&rd->semaphore);
	if (!rd->pending_event)
		goto err_cache_sync;

	num = rd->props.register_num;
	for (i = 0; i < num; i++) {
		if (rd->cache_flag[i] == 1) {
			rc = rt_chip_block_write(rd, rm[i]->addr,
					rm[i]->size, rd->cache_data[i]);
			if (rc < 0) {
				dev_err(&rd->dev, "rt-regmap sync error\n");
				goto err_cache_sync;
			}
			*(rd->cache_flag + i) = 0;
		}
	}
	rd->pending_event = 0;
	dev_info(&rd->dev, "regmap sync successfully\n");
err_cache_sync:
	up(&rd->semaphore);
}
EXPORT_SYMBOL(rt_regmap_cache_sync);

/* rt_regmap_cache_write_back - write current cache data to chip
 * @rd: rt_regmap_device pointer.
 * @reg: register map address
 */
void rt_regmap_cache_write_back(struct rt_regmap_device *rd, u32 reg)
{
	struct reg_index_offset rio;
	const rt_register_map_t *rm = rd->props.rm;
	int rc;

	rio = find_register_index(rd, reg);
	if (rio.index < 0) {
		dev_err(&rd->dev, "reg 0x%02x is out of range\n", reg);
		return;
	}

	down(&rd->semaphore);
	if ((rm[rio.index]->reg_type&RT_REG_TYPE_MASK) != RT_VOLATILE) {
		rc = rt_chip_block_write(rd, rm[rio.index]->addr,
					rm[rio.index]->size,
					rd->cache_data[rio.index]);
		if (rc < 0) {
			dev_err(&rd->dev, "rt-regmap sync error\n");
			goto err_cache_chip_write;
		}
		rd->cache_flag[rio.index] = 0;
	}
	dev_info(&rd->dev, "regmap sync successfully\n");
err_cache_chip_write:
	up(&rd->semaphore);
}
EXPORT_SYMBOL(rt_regmap_cache_write_back);

/* rt_is_reg_volatile - check register map is volatile or not
 * @rd: rt_regmap_device pointer.
 * reg: register map address.
 */
int rt_is_reg_volatile(struct rt_regmap_device *rd, u32 reg)
{
	struct reg_index_offset rio;
	const rt_register_map_t rm;

	rio = find_register_index(rd, reg);
	if (rio.index < 0) {
		dev_err(&rd->dev, "reg 0x%02x is out of range\n", reg);
		return -EINVAL;
	}
	rm = rd->props.rm[rio.index];

	return (rm->reg_type&RT_REG_TYPE_MASK) == RT_VOLATILE ? 1 : 0;
}
EXPORT_SYMBOL(rt_is_reg_volatile);

/* rt_reg_regsize - get register map size for specific register
 * @rd: rt_regmap_device pointer.
 * reg: register map address
 */
int rt_get_regsize(struct rt_regmap_device *rd, u32 reg)
{
	struct reg_index_offset rio;

	rio = find_register_index(rd, reg);
	if (rio.index < 0 || rio.offset != 0) {
		dev_err(&rd->dev, "reg 0x%02x is out of map\n", reg);
		return -EINVAL;
	}
	return rd->props.rm[rio.index]->size;
}
EXPORT_SYMBOL(rt_get_regsize);

static void rt_work_func(struct work_struct *work)
{
	struct rt_regmap_device *rd;

	pr_info(" %s\n", __func__);
	rd = container_of(work, struct rt_regmap_device, rt_work.work);
	rt_regmap_cache_sync(rd);
}

static int rt_chip_block_write(struct rt_regmap_device *rd, u32 reg,
				int bytes, const void *src)
{
	int ret;

	if ((rd->props.rt_regmap_mode & RT_IO_BLK_MODE_MASK) == RT_IO_BLK_ALL ||
	    (rd->props.rt_regmap_mode & RT_IO_BLK_MODE_MASK) == RT_IO_BLK_CHIP)
		return 0;

	ret = rd->rops->write_device(rd->client, reg, bytes, src);

	return ret;
}

static int rt_chip_block_read(struct rt_regmap_device *rd, u32 reg,
				int bytes, void *dst)
{
	int ret;

	ret = rd->rops->read_device(rd->client, reg, bytes, dst);
	return ret;
}

static int rt_cache_block_write(struct rt_regmap_device *rd, u32 reg,
						int bytes, const void *data)
{
	int i, j, reg_base = 0, count = 0, ret = 0, size = 0;
	struct reg_index_offset rio;
	unsigned char wdata[64];
	unsigned char wri_data[128];
	unsigned char blk_index;
	const rt_register_map_t rm;

	if (bytes > 64) {
		dev_err(&rd->dev, "over size > 64 bytes\n");
		return -EINVAL;
	}
	memcpy(wdata, data, bytes);

	rio = find_register_index(rd, reg);
	if (rio.index < 0) {
		dev_err(&rd->dev, "reg 0x%02x is out of range\n", reg);
		return -EINVAL;
	}

	reg_base = 0;
	rm = rd->props.rm[rio.index + reg_base];
	while (bytes > 0) {
		size = ((bytes <= (rm->size-rio.offset)) ?
					bytes : rm->size-rio.offset);
		if ((rm->reg_type&RT_REG_TYPE_MASK) == RT_VOLATILE) {
			ret = rt_chip_block_write(rd,
					rm->addr+rio.offset,
					size,
					&wdata[count]);
			if (ret < 0) {
				dev_err(&rd->dev, "rd->rt_block_write fail\n");
				goto ERR;
			}
			count += size;
		} else {
			blk_index = (rd->props.rt_regmap_mode &
					RT_IO_BLK_MODE_MASK)>>3;

			ret = rd->rt_block_write[blk_index]
				(rd, rm, size, &rio, wdata,
				&count, rio.index+reg_base);
			if (ret < 0) {
				dev_err(&rd->dev, "rd->rt_block_write fail\n");
				goto ERR;
			}
		}

		if ((rm->reg_type&RT_REG_TYPE_MASK) != RT_VOLATILE)
			rd->cache_flag[rio.index+reg_base] = 1;

		bytes -= size;
		if (bytes <= 0)
			goto finished;
		reg_base++;
		rm = rd->props.rm[rio.index + reg_base];
		if ((rio.index + reg_base) >= rd->props.register_num) {
			dev_err(&rd->dev, "over regmap size\n");
			goto ERR;
		}
	}
finished:
	if (rd->props.io_log_en) {
		j = 0;
		for (i = 0; i < count; i++)
			j += snprintf(wri_data + j, sizeof(wri_data) - j,
			"%02x,", wdata[i]);
		pr_info("RT_REGMAP [WRITE] reg0x%04x  [Data] 0x%s\n",
							reg, wri_data);
	}
	return 0;
ERR:
	return -EIO;
}

static int rt_asyn_cache_block_write(struct rt_regmap_device *rd, u32 reg,
						int bytes, const void *data)
{
	int i, j, reg_base, count = 0, ret = 0, size = 0;
	struct reg_index_offset rio;
	unsigned char wdata[64];
	unsigned char wri_data[128];
	unsigned char blk_index;
	const rt_register_map_t rm;

	memcpy(wdata, data, bytes);

	cancel_delayed_work_sync(&rd->rt_work);

	rio = find_register_index(rd, reg);
	if (rio.index < 0) {
		dev_err(&rd->dev, "reg 0x%02x is out of range\n", reg);
		return -EINVAL;
	}

	reg_base = 0;
	rm = rd->props.rm[rio.index + reg_base];
	while (bytes > 0) {
		size = ((bytes <= (rm->size-rio.offset)) ?
					bytes : rm->size-rio.offset);
		if ((rm->reg_type&RT_REG_TYPE_MASK) == RT_VOLATILE) {
			ret = rt_chip_block_write(rd,
					rm->addr+rio.offset, size,
					&wdata[count]);
			count += size;
		} else {
			blk_index = (rd->props.rt_regmap_mode &
					RT_IO_BLK_MODE_MASK)>>3;
			ret = rd->rt_block_write[blk_index]
				(rd, rm, size, &rio, wdata,
				&count, rio.index+reg_base);
		}
		if (ret < 0) {
			dev_err(&rd->dev, "rd->rt_block_write fail\n");
			goto ERR;
		}

		if ((rm->reg_type&RT_REG_TYPE_MASK) != RT_VOLATILE) {
			rd->cache_flag[rio.index+reg_base] = 1;
			rd->pending_event = 1;
		}

		bytes -= size;
		if (bytes <= 0)
			goto finished;
		reg_base++;
		rm = rd->props.rm[rio.index + reg_base];
		rio.offset = 0;
		if ((rio.index + reg_base) >= rd->props.register_num) {
			dev_err(&rd->dev, "over regmap size\n");
			goto ERR;
		}
	}
finished:
	if (rd->props.io_log_en) {
		j = 0;
		for (i = 0; i < count; i++)
			j += snprintf(wri_data + j, sizeof(wri_data) - j,
			"%02x,", wdata[i]);
		pr_info("RT_REGMAP [WRITE] reg0x%04x  [Data] 0x%s\n",
								reg, wri_data);
	}

	schedule_delayed_work(&rd->rt_work, msecs_to_jiffies(1));
	return 0;
ERR:
	return -EIO;
}

static int rt_block_write_blk_all(struct rt_regmap_device *rd,
				  const struct rt_register *rm, int size,
				  const struct reg_index_offset *rio,
				  unsigned char *wdata, int *count,
				  int cache_idx)
{
	down(&rd->write_mode_lock);
	*count += size;
	up(&rd->write_mode_lock);
	return 0;
}

static int rt_block_write_blk_chip(struct rt_regmap_device *rd,
				   const struct rt_register *rm, int size,
				   const struct reg_index_offset *rio,
				   unsigned char *wdata, int *count,
				   int cache_idx)
{
	int i;

	down(&rd->write_mode_lock);
	for (i = rio->offset; i < rio->offset+size; i++) {
		if ((rm->reg_type&RT_REG_TYPE_MASK) != RT_VOLATILE) {
			rd->cache_data[cache_idx][i] =
				wdata[*count] & rm->wbit_mask[i];
			if (!rd->cached[cache_idx])
				rd->cached[cache_idx] = 1;
		}
		*count = *count + 1;
	}
	up(&rd->write_mode_lock);
	return 0;
}

static int rt_block_write_blk_cache(struct rt_regmap_device *rd,
				    const struct rt_register *rm, int size,
				    const struct reg_index_offset *rio,
				    unsigned char *wdata, int *count,
				    int cache_idx)
{
	int ret, cnt;

	down(&rd->write_mode_lock);
	cnt = *count;

	ret = rt_chip_block_write(rd, rm->addr+rio->offset, size, &wdata[cnt]);
	if (ret < 0) {
		dev_err(&rd->dev,
		"rt block write fail at 0x%02x\n", rm->addr + rio->offset);
		up(&rd->write_mode_lock);
		return -EIO;
	}
	cnt += size;
	*count = cnt;
	up(&rd->write_mode_lock);
	return 0;
}

static int rt_block_write(struct rt_regmap_device *rd,
			  const struct rt_register *rm, int size,
			  const struct reg_index_offset *rio,
			  unsigned char *wdata, int *count, int cache_idx)
{
	int i, ret = 0, cnt, change = 0;

	down(&rd->write_mode_lock);
	cnt = *count;

	if (!rd->cached[cache_idx]) {
		for (i = rio->offset; i < size+rio->offset; i++) {
			if ((rm->reg_type & RT_REG_TYPE_MASK) != RT_VOLATILE) {
				rd->cache_data[cache_idx][i] =
					wdata[cnt] & rm->wbit_mask[i];
			}
			cnt++;
		}
		rd->cached[cache_idx] = 1;
		change++;
	} else {
		for (i = rio->offset; i < size+rio->offset; i++) {
			if ((rm->reg_type & RT_REG_TYPE_MASK) != RT_VOLATILE) {
				if (rm->reg_type&RT_WR_ONCE) {
					if (rd->cache_data[cache_idx][i] !=
						(wdata[cnt]&rm->wbit_mask[i]))
						change++;
				}
				rd->cache_data[cache_idx][i] =
					wdata[cnt] & rm->wbit_mask[i];
			}
			cnt++;
		}
	}

	if (!change && (rm->reg_type&RT_WR_ONCE))
		goto finish;

	ret = rt_chip_block_write(rd,
		rm->addr+rio->offset, size, rd->cache_data[cache_idx]);
	if (ret < 0)
		dev_err(&rd->dev, "rt block write fail at 0x%02x\n",
						rm->addr + rio->offset);
finish:
	*count = cnt;
	up(&rd->write_mode_lock);
	return ret;
}

static int (*rt_block_map[])(struct rt_regmap_device *rd,
			     const struct rt_register *rm, int size,
			     const struct reg_index_offset *rio,
			     unsigned char *wdata, int *count,
			     int cache_idx) = {
	&rt_block_write,
	&rt_block_write_blk_all,
	&rt_block_write_blk_cache,
	&rt_block_write_blk_chip,
};

static int rt_cache_block_read(struct rt_regmap_device *rd, u32 reg,
			int bytes, void *dest)
{
	int i, ret, count = 0, reg_base = 0, total_bytes = 0;
	struct reg_index_offset rio;
	const rt_register_map_t rm;
	unsigned char data[100];
	unsigned char tmp_data[32];

	rio = find_register_index(rd, reg);
	if (rio.index < 0) {
		dev_err(&rd->dev, "reg 0x%02x is out of range\n", reg);
		return -EINVAL;
	}

	rm = rd->props.rm[rio.index];

	total_bytes += (rm->size - rio.offset);

	for (i = rio.index+1; i < rd->props.register_num; i++)
		total_bytes += rd->props.rm[i]->size;

	if (bytes > total_bytes) {
		dev_err(&rd->dev, "out of cache map range\n");
		return -EINVAL;
	}

	memcpy(data, &rd->cache_data[rio.index][rio.offset], bytes);

	if ((rm->reg_type&RT_REG_TYPE_MASK) == RT_VOLATILE
					|| rd->cached[rio.index] == 0) {
		ret = rd->rops->read_device(rd->client,
				rm->addr, rm->size, tmp_data);
		if (ret < 0) {
			dev_err(&rd->dev,
			"rt_regmap Error at 0x%02x\n", rm->addr);
			return -EIO;
		}
		for (i = rio.offset; i < rm->size; i++) {
			data[count] = tmp_data[i];
			count++;
		}
		if (!rd->cached[rio.index]) {
			memcpy(rd->cache_data[rio.index], &tmp_data, rm->size);
			rd->cached[rio.index] = 1;
		}
	} else
		count += (rm->size - rio.offset);

	while (count < bytes) {
		reg_base++;
		rm = rd->props.rm[rio.index + reg_base];
		if ((rm->reg_type&RT_REG_TYPE_MASK) == RT_VOLATILE ||
				rd->cached[rio.index+reg_base] == 0) {
			ret = rd->rops->read_device(rd->client,
					rm->addr, rm->size, &data[count]);
			if (ret < 0) {
				dev_err(&rd->dev,
				"rt_regmap Error at 0x%02x\n", rm->addr);
				return -EIO;
			}
			if (!rd->cached[rio.index+reg_base]) {
				memcpy(rd->cache_data[rio.index+reg_base],
						&data[count], rm->size);
				rd->cached[rio.index+reg_base] = 1;
			}
		}
		count += rm->size;
	}

	if (rd->props.io_log_en)
		pr_info("RT_REGMAP [READ] reg0x%04x\n", reg);

	memcpy(dest, data, bytes);

	return 0;
}

/* rt_regmap_cache_backup - back up all cache register value*/
void rt_regmap_cache_backup(struct rt_regmap_device *rd)
{
	const rt_register_map_t *rm = rd->props.rm;
	int i;

	down(&rd->semaphore);
	for (i = 0; i < rd->props.register_num; i++)
		if ((rm[i]->reg_type&RT_REG_TYPE_MASK) != RT_VOLATILE)
			rd->cache_flag[i] = 1;
	rd->pending_event = 1;
	up(&rd->semaphore);
}
EXPORT_SYMBOL(rt_regmap_cache_backup);

/* _rt_regmap_reg_write - write data to specific register map
 * only support 1, 2, 4 bytes regisetr map
 * @rd: rt_regmap_device pointer.
 * @rrd: rt_reg_data pointer.
 */
static int _rt_regmap_reg_write(struct rt_regmap_device *rd,
				struct rt_reg_data *rrd)
{
	const rt_register_map_t *rm = rd->props.rm;
	struct reg_index_offset rio;
	int ret, tmp_data;

	rio = find_register_index(rd, rrd->reg);
	if (rio.index < 0 || rio.offset != 0) {
		dev_err(&rd->dev, "reg 0x%02x is out of regmap\n", rrd->reg);
		return -EINVAL;
	}

	down(&rd->semaphore);
	switch (rm[rio.index]->size) {
	case 1:
		ret = rd->regmap_ops.regmap_block_write(rd,
				rrd->reg, 1, &rrd->rt_data.data_u8);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block write fail\n");
			up(&rd->semaphore);
			return -EIO;
		}
		break;
	case 2:
		if (rd->props.rt_format == RT_LITTLE_ENDIAN)
			tmp_data = be16_to_cpu(rrd->rt_data.data_u32);
		ret = rd->regmap_ops.regmap_block_write(rd,
				rrd->reg, rm[rio.index]->size, &tmp_data);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block write fail\n");
			up(&rd->semaphore);
			return -EIO;
		}
		break;
	case 3:
		if (rd->props.rt_format == RT_LITTLE_ENDIAN) {
			tmp_data = be32_to_cpu(rrd->rt_data.data_u32);
			tmp_data >>= 8;
		}
		ret = rd->regmap_ops.regmap_block_write(rd,
			rrd->reg, rm[rio.index]->size, &tmp_data);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block write fail\n");
			up(&rd->semaphore);
			return -EIO;
		}
		break;
	case 4:
		if (rd->props.rt_format == RT_LITTLE_ENDIAN)
			tmp_data = be32_to_cpu(rrd->rt_data.data_u32);
		ret = rd->regmap_ops.regmap_block_write(rd,
			rrd->reg, rm[rio.index]->size, &tmp_data);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block write fail\n");
			up(&rd->semaphore);
			return -EIO;
		}
		break;
	default:
		dev_err(&rd->dev,
			"Failed: only support 1~4 bytes regmap write\n");
		break;
	}
	up(&rd->semaphore);
	return 0;
}

int rt_regmap_reg_write(struct rt_regmap_device *rd,
		struct rt_reg_data *rrd, u32 reg, const u32 data)
{
	rrd->reg = reg;
	rrd->rt_data.data_u32 = data;
	return _rt_regmap_reg_write(rd, rrd);
}
EXPORT_SYMBOL(rt_regmap_reg_write);

/* _rt_asyn_regmap_reg_write - asyn write data to specific register map*/
static int _rt_asyn_regmap_reg_write(struct rt_regmap_device *rd,
				struct rt_reg_data *rrd)
{
	const rt_register_map_t *rm = rd->props.rm;
	struct reg_index_offset rio;
	int ret, tmp_data = 0;

	rio = find_register_index(rd, rrd->reg);
	if (rio.index < 0 || rio.offset != 0) {
		dev_err(&rd->dev, "reg 0x%02x is out of regmap\n", rrd->reg);
		return -EINVAL;
	}

	down(&rd->semaphore);
	switch (rm[rio.index]->size) {
	case 1:
		ret = rt_asyn_cache_block_write(rd,
				rrd->reg, 1, &rrd->rt_data.data_u8);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block write fail\n");
			ret = -EIO;
			goto err_regmap_write;
		}
		break;
	case 2:
		if (rd->props.rt_format == RT_LITTLE_ENDIAN)
			tmp_data = be16_to_cpu(rrd->rt_data.data_u32);
		ret = rt_asyn_cache_block_write(rd,
				rrd->reg, rm[rio.index]->size, &tmp_data);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block write fail\n");
			ret = -EIO;
			goto err_regmap_write;
		}
		break;
	case 3:
		if (rd->props.rt_format == RT_LITTLE_ENDIAN) {
			tmp_data = be32_to_cpu(rrd->rt_data.data_u32);
			tmp_data >>= 8;
		}
		ret = rt_asyn_cache_block_write(rd,
				rrd->reg, rm[rio.index]->size, &tmp_data);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block write fail\n");
			ret = -EIO;
			goto err_regmap_write;
		}
		break;
	case 4:
		if (rd->props.rt_format == RT_LITTLE_ENDIAN)
			tmp_data = be32_to_cpu(rrd->rt_data.data_u32);
		ret = rt_asyn_cache_block_write(rd,
				rrd->reg, rm[rio.index]->size, &tmp_data);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block write fail\n");
			ret = -EIO;
			goto err_regmap_write;
		}
		break;
	default:
		dev_err(&rd->dev,
			"Failed: only support 1~4 bytes regmap write\n");
		break;
	}
	up(&rd->semaphore);
	return 0;
err_regmap_write:
	up(&rd->semaphore);
	return ret;
}

int rt_asyn_regmap_reg_write(struct rt_regmap_device *rd,
		struct rt_reg_data *rrd, u32 reg, const u32 data)
{
	rrd->reg = reg;
	rrd->rt_data.data_u32 = data;
	return _rt_asyn_regmap_reg_write(rd, rrd);
}
EXPORT_SYMBOL(rt_asyn_regmap_reg_write);

/* _rt_regmap_update_bits - assign bits specific register map */
static int _rt_regmap_update_bits(struct rt_regmap_device *rd,
				struct rt_reg_data *rrd)
{
	const rt_register_map_t *rm = rd->props.rm;
	struct reg_index_offset rio;
	int ret, new, old;
	bool change = false;

	rio = find_register_index(rd, rrd->reg);
	if (rio.index < 0 || rio.offset != 0) {
		dev_err(&rd->dev, "reg 0x%02x is out of regmap\n", rrd->reg);
		return -EINVAL;
	}

	down(&rd->semaphore);
	switch (rm[rio.index]->size) {
	case 1:
		ret = rd->regmap_ops.regmap_block_read(rd,
					rrd->reg, 1, &old);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block read fail\n");
			goto err_update_bits;
		}
		new = (old & ~(rrd->mask)) | (rrd->rt_data.data_u8 & rrd->mask);
		change = old != new;

		if (((rm[rio.index]->reg_type & RT_WR_ONCE) && change) ||
			!(rm[rio.index]->reg_type & RT_WR_ONCE)) {
			ret = rd->regmap_ops.regmap_block_write(rd,
							rrd->reg, 1, &new);
			if (ret < 0) {
				dev_err(&rd->dev,
					"rt regmap block write fail\n");
				goto err_update_bits;
			}
		}
		break;
	case 2:
		ret = rd->regmap_ops.regmap_block_read(rd,
				rrd->reg, rm[rio.index]->size, &old);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block read fail\n");
			goto err_update_bits;
		}
		if (rd->props.rt_format == RT_LITTLE_ENDIAN)
			old = be16_to_cpu(old);

		new = (old & ~(rrd->mask)) |
				(rrd->rt_data.data_u16 & rrd->mask);

		change = old != new;
		if (((rm[rio.index]->reg_type & RT_WR_ONCE) && change) ||
			!(rm[rio.index]->reg_type & RT_WR_ONCE)) {
			if (rd->props.rt_format == RT_LITTLE_ENDIAN)
				new = be16_to_cpu(new);
			ret = rd->regmap_ops.regmap_block_write(rd,
				rrd->reg, rm[rio.index]->size, &new);
			if (ret < 0) {
				dev_err(&rd->dev,
					"rt regmap block write fail\n");
				goto err_update_bits;
			}
		}
		break;
	case 3:
		ret = rd->regmap_ops.regmap_block_read(rd,
				rrd->reg, rm[rio.index]->size, &old);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block read fail\n");
			goto err_update_bits;
		}
		if (rd->props.rt_format == RT_LITTLE_ENDIAN) {
			old = be32_to_cpu(old);
			old >>= 8;
		}

		new = (old & ~(rrd->mask)) |
				(rrd->rt_data.data_u32 & rrd->mask);
		change = old != new;
		if (((rm[rio.index]->reg_type & RT_WR_ONCE) && change) ||
			!(rm[rio.index]->reg_type & RT_WR_ONCE)) {
			if (rd->props.rt_format == RT_LITTLE_ENDIAN) {
				new <<= 8;
				new = be32_to_cpu(new);
			}
			ret = rd->regmap_ops.regmap_block_write(rd,
				rrd->reg, rm[rio.index]->size, &new);
			if (ret < 0) {
				dev_err(&rd->dev,
					"rt regmap block write fail\n");
				goto err_update_bits;
			}
		}
		break;
	case 4:
		ret = rd->regmap_ops.regmap_block_read(rd,
				rrd->reg, rm[rio.index]->size, &old);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block read fail\n");
			goto err_update_bits;
		}
		if (rd->props.rt_format == RT_LITTLE_ENDIAN)
			old = be32_to_cpu(old);

		new = (old & ~(rrd->mask)) |
				(rrd->rt_data.data_u32 & rrd->mask);
		change = old != new;
		if (((rm[rio.index]->reg_type & RT_WR_ONCE) && change) ||
			!(rm[rio.index]->reg_type & RT_WR_ONCE)) {
			if (rd->props.rt_format == RT_LITTLE_ENDIAN)
				new = be32_to_cpu(new);
			ret = rd->regmap_ops.regmap_block_write(rd,
				rrd->reg, rm[rio.index]->size, &new);
			if (ret < 0) {
				dev_err(&rd->dev,
					"rt regmap block write fail\n");
				goto err_update_bits;
			}
		}
		break;
	default:
		dev_err(&rd->dev,
			"Failed: only support 1~4 bytes regmap write\n");
		break;
	}
	up(&rd->semaphore);
	return change;
err_update_bits:
	up(&rd->semaphore);
	return ret;
}

int rt_regmap_update_bits(struct rt_regmap_device *rd,
		struct rt_reg_data *rrd, u32 reg, u32 mask, u32 data)
{
	rrd->reg = reg;
	rrd->mask = mask;
	rrd->rt_data.data_u32 = data;
	return _rt_regmap_update_bits(rd, rrd);
}
EXPORT_SYMBOL(rt_regmap_update_bits);

/* rt_regmap_block_write - block write data to register
 * @rd: rt_regmap_device pointer
 * @reg: register address
 * bytes: leng for write
 * src: source data
 */
int rt_regmap_block_write(struct rt_regmap_device *rd, u32 reg,
				int bytes, const void *src)
{
	int ret;

	down(&rd->semaphore);
	ret = rd->regmap_ops.regmap_block_write(rd, reg, bytes, src);
	up(&rd->semaphore);
	return ret;
};
EXPORT_SYMBOL(rt_regmap_block_write);

/* rt_asyn_regmap_block_write - asyn block write*/
int rt_asyn_regmap_block_write(struct rt_regmap_device *rd, u32 reg,
					int bytes, const void *src)
{
	int ret;

	down(&rd->semaphore);
	ret = rt_asyn_cache_block_write(rd, reg, bytes, src);
	up(&rd->semaphore);
	return ret;
};
EXPORT_SYMBOL(rt_asyn_regmap_block_write);

/* rt_regmap_block_read - block read data form register
 * @rd: rt_regmap_device pointer
 * @reg: register address
 * @bytes: read length
 * @dst: destination for read data
 */
int rt_regmap_block_read(struct rt_regmap_device *rd, u32 reg,
				int bytes, void *dst)
{
	int ret;

	down(&rd->semaphore);
	ret = rd->regmap_ops.regmap_block_read(rd, reg, bytes, dst);
	up(&rd->semaphore);
	return ret;
};
EXPORT_SYMBOL(rt_regmap_block_read);

/* _rt_regmap_reg_read - register read for specific register map
 * only support 1, 2, 4 bytes register map.
 * @rd: rt_regmap_device pointer.
 * @rrd: rt_reg_data pointer.
 */
static int _rt_regmap_reg_read(
		struct rt_regmap_device *rd, struct rt_reg_data *rrd)
{
	const rt_register_map_t *rm = rd->props.rm;
	struct reg_index_offset rio;
	int ret, tmp_data = 0;

	rio = find_register_index(rd, rrd->reg);
	if (rio.index < 0 || rio.offset != 0) {
		dev_err(&rd->dev, "reg 0x%02x is out of regmap\n", rrd->reg);
		return -EINVAL;
	}

	down(&rd->semaphore);
	switch (rm[rio.index]->size) {
	case 1:
		ret = rd->regmap_ops.regmap_block_read(rd,
			rrd->reg, 1, &rrd->rt_data.data_u8);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block read fail\n");
			goto err_regmap_reg_read;
		}
		break;
	case 2:
		ret = rd->regmap_ops.regmap_block_read(rd,
			rrd->reg, rm[rio.index]->size, &tmp_data);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block read fail\n");
			goto err_regmap_reg_read;
		}
		if (rd->props.rt_format == RT_LITTLE_ENDIAN)
			tmp_data = be16_to_cpu(tmp_data);
		rrd->rt_data.data_u16 = tmp_data;
		break;
	case 3:
		ret = rd->regmap_ops.regmap_block_read(rd,
			rrd->reg, rm[rio.index]->size, &tmp_data);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block read fail\n");
			goto err_regmap_reg_read;
		}
		if (rd->props.rt_format == RT_LITTLE_ENDIAN)
			tmp_data = be32_to_cpu(tmp_data);
		rrd->rt_data.data_u32 = (tmp_data >> 8);
		break;
	case 4:
		ret = rd->regmap_ops.regmap_block_read(rd,
			rrd->reg, rm[rio.index]->size, &tmp_data);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block read fail\n");
			goto err_regmap_reg_read;
		}
		if (rd->props.rt_format == RT_LITTLE_ENDIAN)
			tmp_data = be32_to_cpu(tmp_data);
		rrd->rt_data.data_u32 = tmp_data;
		break;
	default:
		dev_err(&rd->dev,
			"Failed: only support 1~4 bytes regmap read\n");
		break;
	}
	up(&rd->semaphore);
	return 0;
err_regmap_reg_read:
	up(&rd->semaphore);
	return ret;
}

int rt_regmap_reg_read(struct rt_regmap_device *rd,
			struct rt_reg_data *rrd, u32 reg)
{
	rrd->reg = reg;
	return _rt_regmap_reg_read(rd, rrd);
}
EXPORT_SYMBOL(rt_regmap_reg_read);

void rt_cache_getlasterror(struct rt_regmap_device *rd, char *buf)
{
	down(&rd->semaphore);
	snprintf(buf, PAGE_SIZE, "%s\n", rd->err_msg);
	up(&rd->semaphore);
}
EXPORT_SYMBOL(rt_cache_getlasterror);

void rt_cache_clrlasterror(struct rt_regmap_device *rd)
{
	down(&rd->semaphore);
	rd->error_occurred = 0;
	snprintf(rd->err_msg, PAGE_SIZE, "%s", "No Error");
	up(&rd->semaphore);
}
EXPORT_SYMBOL(rt_cache_clrlasterror);

/* initialize cache data from rt_register */
int rt_regmap_cache_init(struct rt_regmap_device *rd)
{
	int i, j, bytes_num = 0, count = 0;
	const rt_register_map_t *rm = rd->props.rm;

	dev_info(&rd->dev, "rt register cache data init\n");

	down(&rd->semaphore);
	rd->cache_flag = devm_kzalloc(&rd->dev,
		rd->props.register_num * sizeof(unsigned char), GFP_KERNEL);
	rd->cached = devm_kzalloc(&rd->dev,
		rd->props.register_num * sizeof(unsigned char), GFP_KERNEL);
	rd->cache_data = devm_kzalloc(&rd->dev,
		rd->props.register_num * sizeof(unsigned char *), GFP_KERNEL);

	if (rd->props.group == NULL) {
		rd->props.group = devm_kzalloc(&rd->dev,
				sizeof(*rd->props.group), GFP_KERNEL);
		rd->props.group[0].start = 0x00;
		rd->props.group[0].end = 0xffff;
		rd->props.group[0].mode = RT_1BYTE_MODE;
	}

	for (i = 0; i < rd->props.register_num; i++)
		bytes_num += rm[i]->size;

	rd->alloc_data = devm_kzalloc(&rd->dev,
		bytes_num * sizeof(unsigned char), GFP_KERNEL);

	/* reload cache data from real chip */
	for (i = 0; i < rd->props.register_num; i++) {
		rd->cache_data[i] = rd->alloc_data + count;
		count += rm[i]->size;
		memset(rd->cache_data[i], 0x00, rm[i]->size);
		rd->cache_flag[i] = rd->cached[i] = 0;
	}

	/* set 0xff writeable mask for NORMAL and RESERVE type */
	for (i = 0; i < rd->props.register_num; i++) {
		if ((rm[i]->reg_type & RT_REG_TYPE_MASK) == RT_NORMAL ||
		    (rm[i]->reg_type & RT_REG_TYPE_MASK) == RT_RESERVE) {
			for (j = 0; j < rm[i]->size; j++)
				rm[i]->wbit_mask[j] = 0xff;
		}
	}

	rd->cache_inited = 1;
	dev_info(&rd->dev, "cache cata init successfully\n");
	up(&rd->semaphore);
	return 0;
}
EXPORT_SYMBOL(rt_regmap_cache_init);

/* rt_regmap_cache_reload - reload cache valuew from real chip*/
int rt_regmap_cache_reload(struct rt_regmap_device *rd)
{
	int i;

	down(&rd->semaphore);
	for (i = 0; i < rd->props.register_num; i++)
		rd->cached[i] = rd->cache_flag[i] = 0;
	rd->pending_event = 0;
	up(&rd->semaphore);
	dev_info(&rd->dev, "cache data reload\n");
	return 0;
}
EXPORT_SYMBOL(rt_regmap_cache_reload);

/* rt_regmap_add_debubfs - add user own debugfs node
 * @rd: rt_regmap_devcie pointer.
 * @name: a pointer to a string containing the name of the file to create.
 * @mode: the permission that the file should have.
 * @data: a pointer to something that the caller will want to get to later on.
 *	The inode.i_private pointer will point this value on the open() call.
 * @fops: a pointer to a struct file_operations that should be used for
 *	this file.
 */
int rt_regmap_add_debugfs(struct rt_regmap_device *rd, const char *name,
			  umode_t mode, void *data,
			  const struct file_operations *fops)
{
	return 0;
}
EXPORT_SYMBOL(rt_regmap_add_debugfs);

/* release cache data*/
static void rt_regmap_cache_release(struct rt_regmap_device *rd)
{
	int i;

	dev_info(&rd->dev, "cache data release\n");
	for (i = 0; i < rd->props.register_num; i++)
		rd->cache_data[i] = NULL;
	devm_kfree(&rd->dev, rd->alloc_data);
	if (rd->cache_flag)
		devm_kfree(&rd->dev, rd->cache_flag);
	if (rd->cached)
		devm_kfree(&rd->dev, rd->cached);
	rd->cache_inited = 0;
}

static void rt_regmap_set_cache_mode(
			struct rt_regmap_device *rd, unsigned char mode)
{
	unsigned char mode_mask;

	mode_mask = mode & RT_CACHE_MODE_MASK;
	dev_info(&rd->dev, "%s mode = %d\n", __func__, mode_mask>>1);

	down(&rd->write_mode_lock);
	if (mode_mask == RT_CACHE_WR_THROUGH) {
		rt_regmap_cache_reload(rd);
		rd->regmap_ops.regmap_block_write =
			rt_cache_block_write;
		rd->regmap_ops.regmap_block_read = &rt_cache_block_read;
	} else if (mode_mask == RT_CACHE_WR_BACK) {
		rt_regmap_cache_reload(rd);
		rd->regmap_ops.regmap_block_write =
			rt_asyn_cache_block_write;
		rd->regmap_ops.regmap_block_read = &rt_cache_block_read;
	} else if (mode_mask == RT_CACHE_DISABLE) {
		rd->regmap_ops.regmap_block_write =
			rt_chip_block_write;
		rd->regmap_ops.regmap_block_read = rt_chip_block_read;
	} else {
		dev_err(&rd->dev, "%s out of cache mode index\n", __func__);
		goto mode_err;
	}

	rd->props.rt_regmap_mode &= ~RT_CACHE_MODE_MASK;
	rd->props.rt_regmap_mode |= mode_mask;
mode_err:
	up(&rd->write_mode_lock);
}


static void rt_regmap_device_release(struct device *dev)
{
	struct rt_regmap_device *rd = to_rt_regmap_device(dev);

	devm_kfree(dev, rd);
}

/* check the rt_register format is correct */
static int rt_regmap_check(struct rt_regmap_device *rd)
{
	const rt_register_map_t *rm = rd->props.rm;
	int num = rd->props.register_num;
	int i;

	/* check name property */
	if (!rd->props.name) {
		pr_info("there is no node name for rt-regmap\n");
		return -EINVAL;
	}

	if (!(rd->props.rt_regmap_mode & RT_BYTE_MODE_MASK))
		goto single_byte;

	for (i = 0; i < num; i++) {
		/* check byte size, 1 byte ~ 24 bytes is valid */
		if (rm[i]->size < 1 || rm[i]->size > 24) {
			pr_info("rt register size error at reg 0x%02x\n",
				rm[i]->addr);
			return -EINVAL;
		}
	}

	for (i = 0; i < num - 1; i++) {
		/* check register sequence */
		if (rm[i]->addr >= rm[i + 1]->addr) {
			pr_info("sequence format error at reg 0x%02x\n",
				rm[i]->addr);
			return -EINVAL;
		}
	}

single_byte:
	/* no default reg_addr and reister_map first addr is not 0x00 */
	return 0;
}

static void rt_regmap_watchdog_work(struct work_struct *work)
{
	struct rt_regmap_device *rd = (struct rt_regmap_device *)
		container_of(work,
		struct rt_regmap_device, watchdog_work.work);
	unsigned char current_mode;

	dev_info(&rd->dev, "%s\n", __func__);
	current_mode = rd->props.rt_regmap_mode&RT_CACHE_MODE_MASK;
	if (current_mode != rd->props.cache_mode_ori)
		rt_regmap_set_cache_mode(rd, rd->props.cache_mode_ori);
	else
		dev_info(&rd->dev, "%s same mode, no need change\n", __func__);
	rd->props.watchdog = 0;
}

static enum alarmtimer_restart rt_regmap_watchdog_alarm(
				struct alarm *alarm, ktime_t now)
{
	struct rt_regmap_device *rd = (struct rt_regmap_device *)
		container_of(alarm, struct rt_regmap_device, watchdog_alarm);

	dev_info(&rd->dev, "%s\n", __func__);
	schedule_delayed_work(&rd->watchdog_work, 0);

	return ALARMTIMER_NORESTART;
}

struct rt_regmap_device *rt_regmap_device_register_ex
			(struct rt_regmap_properties *props,
			struct rt_regmap_fops *rops,
			struct device *parent,
			void *client, int slv_addr, void *drvdata)
{
	struct rt_regmap_device *rd;
	int ret = 0, i;
	char device_name[32];
	unsigned char data;

	if (!props) {
		pr_err("%s rt_regmap_properties is NULL\n", __func__);
		return NULL;
	}
	if (!rops) {
		pr_err("%s rt_regmap_fops is NULL\n", __func__);
		return NULL;
	}

	pr_info("regmap_device_register: name = %s\n", props->name);
	rd = devm_kzalloc(parent, sizeof(struct rt_regmap_device), GFP_KERNEL);
	if (!rd) {
		pr_info("rt_regmap_device memory allocate fail\n");
		return NULL;
	}

	/* create a binary semaphore */
	sema_init(&rd->semaphore, 1);
	sema_init(&rd->write_mode_lock, 1);
	rd->dev.parent = parent;
	rd->client = client;
	rd->dev.release = rt_regmap_device_release;
	dev_set_drvdata(&rd->dev, drvdata);
	snprintf(device_name, 32, "rt_regmap_%s", props->name);
	dev_set_name(&rd->dev, device_name);
	memcpy(&rd->props, props, sizeof(struct rt_regmap_properties));
	rd->props.cache_mode_ori = rd->props.rt_regmap_mode&RT_CACHE_MODE_MASK;

	/* check rt_registe_map format */
	ret = rt_regmap_check(rd);
	if (ret) {
		pr_info("rt register map format error\n");
		devm_kfree(parent, rd);
		return NULL;
	}

	ret = device_register(&rd->dev);
	if (ret) {
		pr_info("rt-regmap dev register fail\n");
		devm_kfree(parent, rd);
		return NULL;
	}

	rd->rops = rops;
	rd->slv_addr = slv_addr;
	rd->err_msg = devm_kzalloc(parent, 128*sizeof(char), GFP_KERNEL);

	/* init cache data */
	ret = rt_regmap_cache_init(rd);
	if (ret < 0) {
		pr_info(" rt cache data init fail\n");
		goto err_cacheinit;
	}

	INIT_DELAYED_WORK(&rd->rt_work, rt_work_func);

	for (i = 0; i <= 3; i++)
		rd->rt_block_write[i] = rt_block_map[i];

	data = rd->props.rt_regmap_mode & RT_CACHE_MODE_MASK;
	if (data == RT_CACHE_WR_THROUGH) {
		rd->regmap_ops.regmap_block_write = &rt_cache_block_write;
		rd->regmap_ops.regmap_block_read = &rt_cache_block_read;
	} else if (data == RT_CACHE_WR_BACK) {
		rd->regmap_ops.regmap_block_write = &rt_asyn_cache_block_write;
		rd->regmap_ops.regmap_block_read = &rt_cache_block_read;
	} else if (data == RT_CACHE_DISABLE) {
		rd->regmap_ops.regmap_block_write = &rt_chip_block_write;
		rd->regmap_ops.regmap_block_read = &rt_chip_block_read;
	}

	INIT_DELAYED_WORK(&rd->watchdog_work, rt_regmap_watchdog_work);
	alarm_init(&rd->watchdog_alarm, ALARM_REALTIME,
		rt_regmap_watchdog_alarm);


	return rd;

err_cacheinit:
	device_unregister(&rd->dev);
	return NULL;

}
EXPORT_SYMBOL(rt_regmap_device_register_ex);

/* rt_regmap_device_unregister - unregister rt_regmap_device*/
void rt_regmap_device_unregister(struct rt_regmap_device *rd)
{
	if (!rd)
		return;
	down(&rd->semaphore);
	rd->rops = NULL;
	up(&rd->semaphore);
	if (rd->cache_inited)
		rt_regmap_cache_release(rd);
	device_unregister(&rd->dev);
}
EXPORT_SYMBOL(rt_regmap_device_unregister);

static int __init regmap_plat_init(void)
{
	pr_info("Init Richtek RegMap %s\n", RT_REGMAP_VERSION);
	return 0;
}

subsys_initcall(regmap_plat_init);

static void __exit regmap_plat_exit(void)
{
}

module_exit(regmap_plat_exit);

MODULE_DESCRIPTION("Richtek regmap Driver");
MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_VERSION(RT_REGMAP_VERSION);
MODULE_LICENSE("GPL");
/* Version Note
 * 1.1.14
 *	Fix Coverity by Mandatory's
 */
