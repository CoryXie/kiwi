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
 * @brief		ATA device functions.
 *
 * Reference:
 * - AT Attachment with Packet Interface - 7: Volume 1
 *   http://www.t13.org/Documents/UploadedDocuments/docs2007/
 * - AT Attachment with Packet Interface - 7: Volume 2
 *   http://www.t13.org/Documents/UploadedDocuments/docs2007/
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <assert.h>
#include <console.h>
#include <endian.h>
#include <status.h>

#include "ata_priv.h"

/** Highest block number for LBA-28 transfers. */
#define LBA28_MAX_BLOCK		((uint64_t)1<<28)

/** Highest block number for LBA-28 transfers. */
#define LBA48_MAX_BLOCK		((uint64_t)1<<48)

/** Copy an ATA identification string.
 * @note		Modifies the source string.
 * @param dest		Destination string.
 * @param src		Source string.
 * @param size		Size of the string. */
static void ata_copy_string(char *dest, char *src, size_t size) {
	char *ptr, tmp;
	size_t i;

	ptr = src;
	for(i = 0; i < size; i += 2) {
		tmp = *ptr;
		*ptr = *(ptr + 1);
		ptr++;
		*ptr = tmp;
		ptr++;
	}

	/* Get rid of the trailing spaces. */
	for(i = size - 1; i > 0; i--) {
		if(src[i] != ' ') {
			break;
		}
	}

	memcpy(dest, src, i + 1);
	dest[i + 1] = 0;
}

/** Array of commands. First index = write, second = LBA48, third = DMA. */
static const uint8_t transfer_commands[2][2][2] = {
	{
		{ ATA_CMD_READ_SECTORS, ATA_CMD_READ_DMA },
		{ ATA_CMD_READ_SECTORS_EXT, ATA_CMD_READ_DMA_EXT },
	},
	{
		{ ATA_CMD_WRITE_SECTORS, ATA_CMD_WRITE_DMA },
		{ ATA_CMD_WRITE_SECTORS_EXT, ATA_CMD_WRITE_DMA_EXT },
	},
};

/** Begin an I/O operation.
 * @param device	Device to transfer on.
 * @param buf		Buffer to transfer from/to.
 * @param lba		Block number to start transfer at.
 * @param count		Number of blocks to transfer.
 * @param write		Whether the transfer is a write.
 * @return		Number of blocks that will be transferred. If 0 is
 *			returned, an error occurred. */
static size_t ata_device_begin_io(ata_device_t *device, void *buf, uint64_t lba, size_t count, bool write) {
	ata_channel_t *channel = device->parent;
	status_t ret;

	if(lba < LBA28_MAX_BLOCK) {
		/* Check how many blocks we can transfer. */
		if((lba + count) > LBA28_MAX_BLOCK) {
			count = LBA28_MAX_BLOCK - lba;
		}
		if(count > 256) {
			count = 256;
		}

		/* Prepare the DMA transfer. */
		if(device->dma) {
			ret = ata_channel_prepare_dma(device->parent, buf, count * device->block_size, write);
			if(ret != STATUS_SUCCESS) {
				kprintf(LOG_WARN, "ata: failed to prepare DMA transfer (%d)\n", ret);
				return 0;
			}
		}

		/* Send a NULL to the feature register. */
		ata_channel_write_cmd(channel, ATA_CMD_REG_FEAT, 0);

		/* Write out the number of blocks to read. 0 means 256. */
		ata_channel_write_cmd(channel, ATA_CMD_REG_COUNT, (count == 256) ? 0 : count);

		/* Specify the address of the block. */
		ata_channel_write_cmd(channel, ATA_CMD_REG_LBA_LOW, lba & 0xff);
		ata_channel_write_cmd(channel, ATA_CMD_REG_LBA_MID, (lba >> 8) & 0xff);
		ata_channel_write_cmd(channel, ATA_CMD_REG_LBA_HIGH, (lba >> 16) & 0xff);

		/* Device number with LBA bit set, and last 4 bits of address. */
		ata_channel_write_cmd(channel, ATA_CMD_REG_DEVICE, 0x40 | (device->num << 4) | ((lba >> 24) & 0xf));

		/* Start the transfer. */
		ata_channel_command(device->parent, transfer_commands[write][false][device->dma]);
		return count;
	} else if(lba < LBA48_MAX_BLOCK) {
		if(!device->lba48) {
			kprintf(LOG_WARN, "ata: attempted LBA48 read (%" PRIu64 ") on non-LBA48 device\n", lba);
			return 0;
		}

		/* Check how many blocks we can transfer. FIXME: Because I'm
		 * lazy and haven't made ata_channel_prepare_dma() handle the
		 * case where we have more than the maximum number of blocks
		 * per transfer, limit this to 256. It can actually be 65536. */
		if((lba + count) > LBA48_MAX_BLOCK) {
			count = LBA48_MAX_BLOCK - lba;
		}
		if(count > 256) {
			count = 256;
		}

		/* Prepare the DMA transfer. */
		if(device->dma) {
			ret = ata_channel_prepare_dma(device->parent, buf, count * device->block_size, write);
			if(ret != STATUS_SUCCESS) {
				kprintf(LOG_WARN, "ata: failed to prepare DMA transfer (%d)\n", ret);
				return 0;
			}
		}

		/* Send 2 NULLs to the feature register. */
		ata_channel_write_cmd(channel, ATA_CMD_REG_FEAT, 0);
		ata_channel_write_cmd(channel, ATA_CMD_REG_FEAT, 0);

		/* Write out the number of blocks to read. */
		if(count == 65536) {
			ata_channel_write_cmd(channel, ATA_CMD_REG_COUNT, 0);
			ata_channel_write_cmd(channel, ATA_CMD_REG_COUNT, 0);
		} else {
			ata_channel_write_cmd(channel, ATA_CMD_REG_COUNT, (count >> 8) & 0xff);
			ata_channel_write_cmd(channel, ATA_CMD_REG_COUNT, count & 0xff);
		}

		/* Specify the address of the block. */
		ata_channel_write_cmd(channel, ATA_CMD_REG_LBA_LOW, (lba >> 24) & 0xff);
		ata_channel_write_cmd(channel, ATA_CMD_REG_LBA_LOW, lba & 0xff);
		ata_channel_write_cmd(channel, ATA_CMD_REG_LBA_MID, (lba >> 32) & 0xff);
		ata_channel_write_cmd(channel, ATA_CMD_REG_LBA_MID, (lba >> 8) & 0xff);
		ata_channel_write_cmd(channel, ATA_CMD_REG_LBA_HIGH, (lba >> 40) & 0xff);
		ata_channel_write_cmd(channel, ATA_CMD_REG_LBA_HIGH, (lba >> 16) & 0xff);

		/* Device number with LBA bit set. */
		ata_channel_write_cmd(channel, ATA_CMD_REG_DEVICE, 0x40 | (device->num << 4));

		/* Start the transfer. */
		ata_channel_command(device->parent, transfer_commands[write][true][device->dma]);
		return count;
	} else {
		kprintf(LOG_WARN, "ata: attempted out of range transfer (%" PRIu64 ")\n", lba);
		return 0;
	}
}

/** Perform an I/O operation on an ATA disk.
 * @param device	Device to perform on.
 * @param buf		Buffer to transfer from/to.
 * @param lba		Block number to transfer from/to.
 * @param count		Number of blocks to transfer.
 * @param write		Whether the operation is a write.
 * @return		Status code describing result of the operation. */
static status_t ata_device_io(ata_device_t *device, void *buf, uint64_t lba, size_t count, bool write) {
	uint8_t error, status;
	size_t current, i;
	status_t ret;

	ata_channel_begin_command(device->parent, device->num);

	while(count) {
		/* Set up the address registers and select the device. */
		current = ata_device_begin_io(device, buf, lba, count, write);
		if(!current) {
			ata_channel_finish_command(device->parent);
			return STATUS_DEVICE_ERROR;
		}

		if(device->dma) {
			/* Start the DMA transfer and wait for it to finish. */
			if(!ata_channel_perform_dma(device->parent)) {
				ata_channel_finish_dma(device->parent);
				kprintf(LOG_WARN, "ata: timed out waiting for DMA transfer on %d:%u\n",
				        device->parent->id, device->num);
				ata_channel_finish_command(device->parent);
				return STATUS_DEVICE_ERROR;
			}

			ret = ata_channel_finish_dma(device->parent);
			buf += current * device->block_size;
		} else {
			/* Do a PIO transfer of each sector. */
			for(i = 0; i < current; i++) {
				if(write) {
					ret = ata_channel_write_pio(device->parent, buf, device->block_size);
				} else {
					ret = ata_channel_read_pio(device->parent, buf, device->block_size);
				}
				if(ret != STATUS_SUCCESS) {
					break;
				}

				buf += device->block_size;
			}
		}

		/* Handle failure. */
		if(ret != STATUS_SUCCESS) {
			status = ata_channel_status(device->parent);
			error = ata_channel_error(device->parent);
			kprintf(LOG_WARN, "ata: %s of %zu block(s) from %" PRIu64 " on %d:%u failed "
			        "(ret: %d, status: %u, error: %u)\n", (write) ? "write" : "read",
			        current, lba, device->parent->id, device->num, ret, status, error);
			ata_channel_finish_command(device->parent);
			return STATUS_DEVICE_ERROR;
		}

		count -= current;
		lba += current;
	}

	ata_channel_finish_command(device->parent);
	return STATUS_SUCCESS;
}

/** Read from an ATA disk.
 * @param _device	Device to read from.
 * @param buf		Buffer to read into.
 * @param lba		Block number to read from.
 * @param count		Number of blocks to read.
 * @return		Status code describing result of the operation. */
static status_t ata_disk_read(disk_device_t *_device, void *buf, uint64_t lba, size_t count) {
	ata_device_t *device = _device->data;
	return ata_device_io(device, buf, lba, count, false);
}

/** Write to an ATA device.
 * @param _device	Device to write to.
 * @param buf		Buffer to write from.
 * @param lba		Block number to write to.
 * @param count		Number of blocks to write.
 * @return		Status code describing result of the operation. */
static status_t ata_disk_write(disk_device_t *_device, const void *buf, uint64_t lba, size_t count) {
	ata_device_t *device = _device->data;
	return ata_device_io(device, (void *)buf, lba, count, true);
}

/** ATA disk device operations structure. */
static disk_ops_t ata_disk_ops = {
	.read = ata_disk_read,
	.write = ata_disk_write,
};

/** Detect a device on a channel.
 * @param channel	Channel to detect on.
 * @param num		Device number (0 or 1). */
void ata_device_detect(ata_channel_t *channel, uint8_t num) {
	uint16_t *ident = NULL, word;
	char name[DEVICE_NAME_MAX];
	ata_device_t *device;
	size_t modes = 0;
	uint64_t blocks;
	status_t ret;

	if(ata_channel_begin_command(channel, num) != STATUS_SUCCESS) {
		return;
	}

	/* Send an IDENTIFY DEVICE command. Perform a manual wait as we don't
	 * want to wait too long if the device doesn't exist. */
	ident = kmalloc(512, MM_SLEEP);
	ata_channel_command(channel, ATA_CMD_IDENTIFY);
	if(ata_channel_wait(channel, ATA_STATUS_BSY | ATA_STATUS_DRQ, 0, true,
	                    true, 50000) != STATUS_SUCCESS) {
		goto fail;
	} else if(ata_channel_read_pio(channel, ident, 512) != STATUS_SUCCESS) {
		goto fail;
	}

	/* Check whether we can use the device. */
	if(le16_to_cpu(ident[0]) & (1<<15)) {
		kprintf(LOG_DEBUG, "ata: skipping non-ATA device %d:%u\n", channel->id, num);
		goto fail;
	} else if(!(le16_to_cpu(ident[49]) & (1<<9))) {
		kprintf(LOG_DEBUG, "ata: skipping non-LBA device %d:%u\n", channel->id, num);
		goto fail;
	}

	/* Allocate a device structure and fill it out. */
	device = kmalloc(sizeof(*device), MM_SLEEP);
	ata_copy_string(device->model, (char *)(ident + 27), 40);
	ata_copy_string(device->serial, (char *)(ident + 10), 20);
	ata_copy_string(device->revision, (char *)(ident + 23), 8);
	device->num = num;
	device->parent = channel;
	device->lba48 = (le16_to_cpu(ident[83]) & (1<<10)) ? true : false;
	kprintf(LOG_NORMAL, "ata: found device %u on channel %d:\n", num, channel->id);
	kprintf(LOG_NORMAL, " model:      %s\n", device->model);
	kprintf(LOG_NORMAL, " serial:     %s\n", device->serial);
	kprintf(LOG_NORMAL, " revision:   %s\n", device->revision);
	kprintf(LOG_NORMAL, " lba48:      %d\n", device->lba48);

	/* Get the block count. */
	if(device->lba48) {
		blocks = le64_to_cpu(*(uint64_t *)(ident + 100));
	} else {
		blocks = le32_to_cpu(*(uint32_t *)(ident + 60));
	}
	kprintf(LOG_NORMAL, " blocks:     %u\n", blocks);

	/* Get the block size - "Bit 12 of word 106 shall be set to 1 to
	 * indicate that the device has been formatted with a logical sector
	 * size larger than 256 words." */
	word = le16_to_cpu(ident[106]);
	if(word & (1<<14) && !(word & (1<<15)) && word & (1<<12)) {
		/* Words 117-118: Logical Sector Size. */
		device->block_size = le32_to_cpu(*(uint32_t *)(ident + 117)) * 2;
	} else {
		device->block_size = 512;
	}
	kprintf(LOG_NORMAL, " block_size: %u\n", device->block_size);
	kprintf(LOG_NORMAL, " size:       %llu\n", (uint64_t)blocks * device->block_size);

	/* Detect whether DMA is supported. */
	device->dma = false;
	if(channel->dma && le16_to_cpu(ident[49]) & (1<<8)) {
		/* Get selected DMA modes. */
		word = le16_to_cpu(ident[63]);
		if(word & (1<<8))  { /* Multiword DMA mode 0. */ modes++; }
		if(word & (1<<9))  { /* Multiword DMA mode 1. */ modes++; }
		if(word & (1<<10)) { /* Multiword DMA mode 2. */ modes++; }
		word = le16_to_cpu(ident[88]);
		if(word & (1<<8))  { /* Ultra DMA mode 0. */ modes++; }
		if(word & (1<<9))  { /* Ultra DMA mode 1. */ modes++; }
		if(word & (1<<10)) { /* Ultra DMA mode 2. */ modes++; }
		if(word & (1<<11)) { /* Ultra DMA mode 3. */ modes++; }
		if(word & (1<<12)) { /* Ultra DMA mode 4. */ modes++; }
		if(word & (1<<13)) { /* Ultra DMA mode 5. */ modes++; }
		if(word & (1<<14)) { /* Ultra DMA mode 6. */ modes++; }

		/* Only one mode should be selected. */
		if(modes > 1) {
			kprintf(LOG_WARN, "ata: device %d:%u has more than one DMA mode selected\n",
			        num, channel->id);
		} else if(modes == 1) {
			device->dma = true;
		}
	}
	kprintf(LOG_NORMAL, " dma:        %d\n", device->dma);

	kfree(ident);
	ata_channel_finish_command(channel);

	/* Register the device with the disk device manager. */
	sprintf(name, "%d", num);
	ret = disk_device_create(name, channel->node, &ata_disk_ops, device, blocks,
	                         device->block_size, &device->node);
	if(ret != STATUS_SUCCESS) {
		fatal("Could not create ATA disk device %u (%d)", num, ret);
	}
	return;
fail:
	kfree(ident);
	ata_channel_finish_command(channel);
}
