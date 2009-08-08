/* Kiwi boot image loader
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Boot image loader.
 */

#include <console/kprintf.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <io/vfs.h>

#include <proc/loader.h>
#include <proc/process.h>
#include <proc/thread.h>

#include <bootimg.h>
#include <errors.h>
#include <fatal.h>
#include <module.h>

#if CONFIG_VFS_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Address of boot image provided by the boot loader. */
ptr_t bootimg_addr;

/** Size of boot image. */
size_t bootimg_size;

/** Tar entry types. */
#define REGTYPE		'0'	/**< Regular file (preferred code). */
#define AREGTYPE	'\0'	/**< Regular file (alternate code). */
#define LNKTYPE		'1'	/**< Hard link. */
#define SYMTYPE		'2'	/**< Symbolic link (hard if not supported). */
#define CHRTYPE		'3'	/**< Character special. */
#define BLKTYPE		'4'	/**< Block special. */
#define DIRTYPE		'5'	/**< Directory.  */
#define FIFOTYPE	'6'	/**< Named pipe.  */
#define CONTTYPE	'7'	/**< Contiguous file. */

/** Header for a tar file. */
typedef struct tar_header {
	char name[100];		/**< Name of entry. */
	char mode[8];		/**< Mode of entry. */
	char uid[8];		/**< User ID. */
	char gid[8];		/**< Group ID. */
	char size[12];		/**< Size of entry. */
	char mtime[12];		/**< Modification time. */
	char chksum[8];		/**< Checksum. */
	char typeflag;		/**< Type flag. */
	char linkname[100];	/**< Symbolic link name. */
	char magic[6];		/**< Magic string. */
	char version[2];	/**< TAR version. */
	char uname[32];		/**< User name. */
	char gname[32];		/**< Group name. */
	char devmajor[8];	/**< Device major. */
	char devminor[8];	/**< Device minor. */
	char prefix[155];	/**< Prefix. */
} tar_header_t;

/** Thread to load the startup binary.
 * @param arg1		Pointer to VFS node for binary.
 * @param arg2		Unused. */
static void bootimg_startup_thread(void *arg1, void *arg2) {
	vfs_node_t *node = arg1;
	int ret;

	ret = loader_binary_load(node, NULL, NULL, NULL);
	fatal("Failed to load startup binary (%d)", ret);
}

/** Extract the boot image.
 *
 * Extracts the boot image to the root filesystem (the RamFS mounted by the
 * VFS). By the time this function is called, the architecture or platform
 * should have set the address and size of the boot image.
 *
 * @note		Assumes the current directory is the root of the FS.
 */
void bootimg_load(void) {
	vfs_node_t *node, *startup = NULL;
	ptr_t addr = bootimg_addr;
	tar_header_t *hdr;
	thread_t *thread;
	process_t *proc;
	int64_t size;
	size_t bytes;
	int ret;

	hdr = (tar_header_t *)bootimg_addr;

	/* Loop until we encounter two null bytes (EOF). */
	while(hdr->name[0] && hdr->name[1]) {
		if(strncmp(hdr->magic, "ustar", 5) != 0) {
			fatal("Boot image format is incorrect");
		}

		/* All fields in the header are stored as ASCII - convert the
		 * size to an integer (base 8). */
		size = strtoll(hdr->size, NULL, 8);

		/* Handle the entry based on its type flag. */
		switch(hdr->typeflag) {
		case REGTYPE:
		case AREGTYPE:
			if((ret = vfs_file_create(hdr->name, &node)) != 0) {
				fatal("Failed to create regular file %s (%d)", hdr->name, ret);
			}

			if((ret = vfs_file_write(node, (void *)(addr + 512), size, 0, &bytes)) != 0) {
				fatal("Failed to write file %s (%d)", hdr->name, ret);
			} else if((int64_t)bytes != size) {
				fatal("Did not write all data for file %s (%zu, %zu)", hdr->name, bytes, size);
			}

			dprintf("bootimg: extracted regular file %s (%lld bytes)\n", hdr->name, size);
			if(strcmp(hdr->name, "startup") == 0) {
				startup = node;
			} else {
				vfs_node_release(node);
			}
			break;
		case DIRTYPE:
			if((ret = vfs_dir_create(hdr->name, NULL)) != 0) {
				fatal("Failed to create directory %s (%d)", hdr->name, ret);
			}

			dprintf("bootimg: created directory %s\n", hdr->name);
			break;
		case SYMTYPE:
			if((ret = vfs_symlink_create(hdr->name, hdr->linkname, NULL)) != 0) {
				fatal("Failed to create symbolic link %s (%d)", hdr->name, ret);
			}

			dprintf("bootimg: created symbolic link %s -> %s\n", hdr->name, hdr->linkname);
			break;
		default:
			dprintf("bootimg: unhandled type flag '%c'\n", hdr->typeflag);
			break;
		}

		/* 512 for the header, plus the file size if necessary. */
		addr += 512 + ((size != 0) ? ROUND_UP(size, 512) : 0);
		hdr = (tar_header_t *)addr;
	}

	/* Check if we have a startup binary. */
	if(!startup) {
		fatal("No startup binary found in boot image");
	}

	/* Create process to load the startup binary. Does not require an
	 * address space to begin with as loader_binary_load() will create
	 * one. */
	if((ret = process_create("startup", kernel_proc, PRIORITY_SYSTEM, PROCESS_NOASPACE, &proc)) != 0) {
		fatal("Could not create process to load startup (%d)", ret);
	}

	if((ret = thread_create("startup", proc, 0, bootimg_startup_thread, startup, NULL, &thread)) != 0) {
		fatal("Could not create thread to load startup (%d)", ret);
	}
	thread_run(thread);
}