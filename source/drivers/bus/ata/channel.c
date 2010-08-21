/*
 * Copyright (C) 2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		ATA channel management.
 *
 * Reference:
 * - AT Attachment with Packet Interface - 7: Volume 1
 *   http://www.t13.org/Documents/UploadedDocuments/docs2007/
 * - AT Attachment with Packet Interface - 7: Volume 2
 *   http://www.t13.org/Documents/UploadedDocuments/docs2007/
 */

#include <lib/atomic.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/page.h>

#include <assert.h>
#include <console.h>
#include <module.h>
#include <status.h>
#include <time.h>

#include "ata_priv.h"

/** Next channel ID. */
static atomic_t next_channel_id = 0;

/** Read from a control register.
 * @param channel	Channel to read from.
 * @param reg		Register to read from.
 * @return		Value read. */
uint8_t ata_channel_read_ctrl(ata_channel_t *channel, int reg) {
	assert(channel->ops->read_ctrl);
	return channel->ops->read_ctrl(channel, reg);
}

/** Write to a control register.
 * @param channel	Channel to read from.
 * @param reg		Register to write to.
 * @param val		Value to write. */
void ata_channel_write_ctrl(ata_channel_t *channel, int reg, uint8_t val) {
	assert(channel->ops->write_ctrl);
	channel->ops->write_ctrl(channel, reg, val);
}

/** Read from a command register.
 * @param channel	Channel to read from.
 * @param reg		Register to read from.
 * @return		Value read. */
uint8_t ata_channel_read_cmd(ata_channel_t *channel, int reg) {
	assert(channel->ops->read_cmd);
	return channel->ops->read_cmd(channel, reg);
}

/** Write to a command register.
 * @param channel	Channel to read from.
 * @param reg		Register to write to.
 * @param val		Value to write. */
void ata_channel_write_cmd(ata_channel_t *channel, int reg, uint8_t val) {
	assert(channel->ops->write_cmd);
	channel->ops->write_cmd(channel, reg, val);
}

/** Wait for DRQ and perform a PIO data read.
 * @param channel	Channel to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @return		STATUS_SUCCESS if succeeded, STATUS_DEVICE_ERROR if a
 *			device error occurred, or STATUS_TIMED_OUT if timed out
 *			while waiting for DRQ. */
status_t ata_channel_read_pio(ata_channel_t *channel, void *buf, size_t count) {
	status_t ret;

	assert(channel->ops->read_pio);

	/* Wait for DRQ to be set and BSY to be clear. */
	ret = ata_channel_wait(channel, ATA_STATUS_DRQ, 0, false, true, SECS2USECS(5));
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	channel->ops->read_pio(channel, buf, count);
	return STATUS_SUCCESS;
}

/** Wait for DRQ and perform a PIO data write.
 * @param channel	Channel to write to.
 * @param buf		Buffer to write from.
 * @param count		Number of bytes to write.
 * @return		STATUS_SUCCESS if succeeded, STATUS_DEVICE_ERROR if a
 *			device error occurred, or STATUS_TIMED_OUT if timed out
 *			while waiting for DRQ. */
status_t ata_channel_write_pio(ata_channel_t *channel, const void *buf, size_t count) {
	status_t ret;

	assert(channel->ops->write_pio);

	/* Wait for DRQ to be set and BSY to be clear. */
	ret = ata_channel_wait(channel, ATA_STATUS_DRQ, 0, false, true, SECS2USECS(5));
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	channel->ops->write_pio(channel, buf, count);
	return STATUS_SUCCESS;
}

/** Add an entry to a DMA transfer array.
 * @param vecp		Pointer to pointer to array.
 * @param entriesp	Pointer to entry count.
 * @param addr		Virtual address.
 * @param size		Size of transfer. */
static void add_dma_transfer(ata_dma_transfer_t **vecp, size_t *entriesp, ptr_t addr, size_t size) {
	size_t i = (*entriesp)++;
	phys_ptr_t phys;
	ptr_t pgoff;

	/* Find the physical address. */
	pgoff = addr % PAGE_SIZE;
	page_map_lock(&kernel_page_map);
	if(!page_map_find(&kernel_page_map, addr - pgoff, &phys)) {
		fatal("Part of DMA transfer buffer was not mapped");
	}
	page_map_unlock(&kernel_page_map);

	*vecp = krealloc(*vecp, sizeof(**vecp) * *entriesp, MM_SLEEP);
	(*vecp)[i].phys = phys + pgoff;
	(*vecp)[i].size = size;
}

/** Prepare a DMA transfer.
 * @todo		This needs to properly handle the case where we break
 *			the channel's constraints on DMA data.
 * @todo		This (and the above function) is an abomination.
 * @param channel	Channel to prepare on.
 * @param buf		Buffer to transfer from/to.
 * @param count		Number of bytes to transfer.
 * @param write		Whether the transfer is a write.
 * @return		Status code describing result of operation. */
status_t ata_channel_prepare_dma(ata_channel_t *channel, void *buf, size_t count, bool write) {
	ata_dma_transfer_t *vec = NULL;
	size_t entries = 0, esize;
	ptr_t ibuf = (ptr_t)buf;
	status_t ret;

	/* Align on a page boundary. */
	if(ibuf % PAGE_SIZE) {
		esize = MIN(count, ROUND_UP(ibuf, PAGE_SIZE) - ibuf);
		add_dma_transfer(&vec, &entries, ibuf, esize);
		ibuf += esize;
		count -= esize;
	}

	/* Add whole pages. */
	while(count / PAGE_SIZE) {
		add_dma_transfer(&vec, &entries, ibuf, PAGE_SIZE);
		ibuf += PAGE_SIZE;
		count -= PAGE_SIZE;
	}

	/* Add what's left. */
	if(count) {
		add_dma_transfer(&vec, &entries, ibuf, count);
	}

	if(entries > channel->max_dma_bpt) {
		kprintf(LOG_WARN, "ata: ???\n");
		return STATUS_NOT_IMPLEMENTED;
	}

	/* Prepare the transfer. */
	assert(channel->ops->prepare_dma);
	ret = channel->ops->prepare_dma(channel, vec, entries, write);
	kfree(vec);
	return ret;
}

/** Start a DMA transfer and wait for it to complete.
 * @param channel	Channel to perform transfer on.
 * @return		True if completed, false if timed out. The operation
 *			may not have succeeded - use the result of
 *			ata_channel_finish_dma() to find out if it did. */
bool ata_channel_perform_dma(ata_channel_t *channel) {
	status_t ret;

	/* The IRQ lock is used to guarantee that we're waiting for the IRQ
	 * when the IRQ handler calls condvar_broadcast(). */
	spinlock_lock(&channel->irq_lock);

	/* Enable interrupts. */
	ata_channel_write_ctrl(channel, ATA_CTRL_REG_DEVCTRL, 0);

	/* Start off the transfer. */
	assert(channel->ops->start_dma);
	channel->ops->start_dma(channel);

	/* Wait for an IRQ to arrive. */
	ret = condvar_wait_etc(&channel->irq_cv, NULL, &channel->irq_lock, SECS2USECS(30), 0);
	if(ret != STATUS_SUCCESS) {
		spinlock_unlock(&channel->irq_lock);
		return false;
	}

	/* Disable interrupts again. */
	ata_channel_write_ctrl(channel, ATA_CTRL_REG_DEVCTRL, ATA_DEVCTRL_NIEN);

	spinlock_unlock(&channel->irq_lock);
	return true;
}

/** Clean up after a DMA transfer.
 * @param channel	Channel to clean up on.
 * @return		STATUS_SUCCESS if the DMA transfer was successful,
 *			STATUS_DEVICE_ERROR if not. */
status_t ata_channel_finish_dma(ata_channel_t *channel) {
	assert(channel->ops->finish_dma);
	return channel->ops->finish_dma(channel);
}

/** Get the content of the alternate status register.
 * @param channel	Channel to get from.
 * @return		Value of alternate status register. */
uint8_t ata_channel_status(ata_channel_t *channel) {
	return ata_channel_read_ctrl(channel, ATA_CTRL_REG_ALT_STATUS);
}

/** Get the content of the error register.
 * @param channel	Channel to get from.
 * @return		Value of error register. */
uint8_t ata_channel_error(ata_channel_t *channel) {
	return ata_channel_read_cmd(channel, ATA_CMD_REG_ERR);
}

/** Get the currently selected device.
 * @param channel	Channel to get from.
 * @return		Current selected device. */
uint8_t ata_channel_selected(ata_channel_t *channel) {
	return (ata_channel_read_cmd(channel, ATA_CMD_REG_DEVICE) >> 4) & (1<<0);
}

/** Issue a command to the selected device.
 * @param channel	Channel to perform command on.
 * @param cmd		Command to perform. */
void ata_channel_command(ata_channel_t *channel, uint8_t cmd) {
	ata_channel_write_cmd(channel, ATA_CMD_REG_CMD, cmd);
	spin(1);
}

/** Trigger a software reset of both devices.
 * @param channel	Channel to reset. */
void ata_channel_reset(ata_channel_t *channel) {
	/* See 11.2 - Software reset protocol (in Volume 2). We wait for longer
	 * than necessary to be sure it's done. */
	ata_channel_write_ctrl(channel, ATA_CTRL_REG_DEVCTRL, ATA_DEVCTRL_SRST | ATA_DEVCTRL_NIEN);
	usleep(20);
	ata_channel_write_ctrl(channel, ATA_CTRL_REG_DEVCTRL, ATA_DEVCTRL_NIEN);
	usleep(MSECS2USECS(150));
	ata_channel_wait(channel, 0, 0, false, false, 1000);

	/* Clear any pending interrupts. */
	ata_channel_read_cmd(channel, ATA_CMD_REG_STATUS);
}

/** Wait for device status to change.
 * @note		When BSY is set in the status register, other bits must
 *			be ignored. Therefore, if waiting for BSY, it must be
 *			the only bit specified to wait for (unless any is true).
 *			There is also no need to wait for BSY to be cleared, as
 *			this is done automatically.
 * @param channel	Channel to wait on.
 * @param set		Bits to wait to be set.
 * @param clear		Bits to wait to be clear.
 * @param any		Wait for any bit in set to be set.
 * @param error		Check for errors/faults.
 * @param timeout	Timeout in microseconds.
 * @return		Status code describing result of the operation. */
status_t ata_channel_wait(ata_channel_t *channel, uint8_t set, uint8_t clear, bool any,
                          bool error, useconds_t timeout) {
	uint8_t status;
	useconds_t i;

	assert(timeout);

	/* If waiting for BSY, ensure no other bits are set. Otherwise, add BSY
	 * to the bits to wait to be clear. */
	if(set & ATA_STATUS_BSY) {
		assert(any || (set == ATA_STATUS_BSY && clear == 0));
	} else {
		clear |= ATA_STATUS_BSY;
	}

	while(timeout) {
		status = ata_channel_status(channel);
		if(error) {
			if(!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_ERR || status & ATA_STATUS_DF)) {
				return STATUS_DEVICE_ERROR;
			}
		}
		if(!(status & clear) && ((any && (status & set)) || (status & set) == set)) {
			return STATUS_SUCCESS;
		}

		i = (timeout < 1000) ? timeout : 1000;
		usleep(i);
		timeout -= i;
	}

	return STATUS_TIMED_OUT;
}

/** Prepares to perform a command on a channel.
 *
 * Locks the channel, waits for it to become ready (DRQ and BSY set to 0),
 * selects the specified device and waits for it to become ready again. This
 * implements the HI1:Check_Status and HI2:Device_Select parts of the Bus idle
 * protocol. It should be called prior to performing any command. When the
 * command is finished, ata_channel_finish_command() must be called.
 *
 * @param channel	Channel to perform command on.
 * @param num		Device number to select (0 or 1).
 *
 * @return		Status code describing the result of the operation.
 */
status_t ata_channel_begin_command(ata_channel_t *channel, uint8_t num) {
	bool attempted = false;

	assert(num == 0 || num == 1);

	/* Begin by locking the channel, to prevent other devices on it from
	 * interfering with our operation. */
	mutex_lock(&channel->lock);

	while(true) {
		/* Wait for BSY and DRQ to be cleared (BSY is checked automatically). */
		if(ata_channel_wait(channel, 0, ATA_STATUS_DRQ, false, false, SECS2USECS(5)) != STATUS_SUCCESS) {
			kprintf(LOG_WARN, "ata: timed out while waiting for channel %d to become idle (status: 0x%x)\n",
			        channel->id, ata_channel_status(channel));
			mutex_unlock(&channel->lock);
			return STATUS_DEVICE_ERROR;
		}

		/* Check whether the device is selected. */
		if(ata_channel_selected(channel) == num) {
			return STATUS_SUCCESS;
		}

		/* Fail if we've already attempted to set the device. */
		if(attempted) {
			kprintf(LOG_WARN, "ata: channel %d did not respond to setting device %u\n",
			        channel->id, num);
			mutex_unlock(&channel->lock);
			return STATUS_DEVICE_ERROR;
		}

		/* Try to set it and then wait again. */
		ata_channel_write_cmd(channel, ATA_CMD_REG_DEVICE, num << 4);
		spin(1);
	}
}

/** Releases the channel after a command.
 * @param channel	Channel to finish on. */
void ata_channel_finish_command(ata_channel_t *channel) {
	mutex_unlock(&channel->lock);
}

/** Register a new ATA channel.
 * @param parent	Parent in the device tree.
 * @param ops		Channel operations structure.
 * @param data		Implementation-specific data pointer.
 * @param dma		Whether the channel supports DMA.
 * @param max_dma_bpt	Maximum number of blocks per DMA transfer.
 * @param max_dma_addr	Maximum physical address for a DMA transfer.
 * @return		Pointer to channel structure if added, NULL if not. */
ata_channel_t *ata_channel_add(device_t *parent, ata_channel_ops_t *ops, void *data, bool dma,
                               size_t max_dma_bpt, phys_ptr_t max_dma_addr) {
	device_attr_t attr[] = {
		{ "type", DEVICE_ATTR_STRING, { .string = "ata-channel" } },
	};
	char name[DEVICE_NAME_MAX];
	ata_channel_t *channel;
	status_t ret;

	assert(parent);
	assert(ops);

	/* Create a new channel structure. */
	channel = kmalloc(sizeof(*channel), MM_SLEEP);
	mutex_init(&channel->lock, "ata_channel_lock", 0);
	spinlock_init(&channel->irq_lock, "ata_channel_irq_lock");
	condvar_init(&channel->irq_cv, "ata_channel_irq_cv");
	channel->ops = ops;
	channel->data = data;
	channel->dma = dma;
	channel->max_dma_bpt = max_dma_bpt;
	channel->max_dma_addr = max_dma_addr;

	/* Check presence by writing a value to the low LBA port on the channel,
	 * then reading it back. If the value is the same, it is present. */
	ata_channel_write_cmd(channel, ATA_CMD_REG_LBA_LOW, 0xAB);
	if(ata_channel_read_cmd(channel, ATA_CMD_REG_LBA_LOW) != 0xAB) {
		kfree(channel);
		return NULL;
	}

	/* Allocate an ID for the controller. */
	channel->id = atomic_inc(&next_channel_id);

	/* Publish it in the device tree. */
	sprintf(name, "ata%d", channel->id);
	ret = device_create(name, parent, NULL, NULL, attr, ARRAYSZ(attr), &channel->node);
	if(ret != STATUS_SUCCESS) {
		kprintf(LOG_WARN, "ata: could not create device tree node for channel %d (%d)\n",
			channel->id, ret);
		kfree(channel);
		return NULL;
	}

	/* Reset the channel to a decent state. */
	ata_channel_reset(channel);
	return channel;
}
MODULE_EXPORT(ata_channel_add);

/** Scan an ATA channel for devices.
 * @param channel	Channel to scan. */
void ata_channel_scan(ata_channel_t *channel) {
	ata_device_detect(channel, 0);
	ata_device_detect(channel, 1);
}
MODULE_EXPORT(ata_channel_scan);

/** Handle an ATA channel interrupt.
 * @note		The caller should check that the interrupt belongs to
 *			the channel before calling this.
 * @note		Safe to call from IRQ context.
 * @param channel	Channel that the interrupt occurred on.
 * @return		IRQ result code. */
irq_result_t ata_channel_interrupt(ata_channel_t *channel) {
	bool woken;

	spinlock_lock(&channel->irq_lock);
	woken = condvar_broadcast(&channel->irq_cv);
	spinlock_unlock(&channel->irq_lock);

	if(!woken) {
		kprintf(LOG_WARN, "ata: received spurious interrupt on channel %d\n",
		        channel->id);
	}
	return (woken) ? IRQ_HANDLED : IRQ_UNHANDLED;
}
MODULE_EXPORT(ata_channel_interrupt);