// SPDX-License-Identifier: GPL-2.0+
/*
 * Bootmethod for FIT images in the style of Boot Loader Specification Type #2 from a block device
 *
 * Copyright 2026 Refeyn Ltd
 */

#define LOG_CATEGORY UCLASS_BOOTSTD

#include <common.h>
#include <bootm.h>
#include <bootdev.h>
#include <bootflow.h>
#include <bootmeth.h>
#include <bootstd.h>
#include <command.h>
#include <dm.h>
#include <fs.h>
#include <malloc.h>
#include <mapmem.h>
#include <mmc.h>
#include <linux/ctype.h>

static int fit_blst2_check(struct udevice *dev, struct bootflow_iter *iter)
{
    int ret;

    /* This only works on block devices */
    ret = bootflow_iter_check_blk(iter);
    if (ret)
        return log_msg_ret("blk", ret);

    return 0;
}

struct filename_parse_result {
    int tries_total;
    int tries_left;
    char tryless_path[256];
};

static int str_rfind(const char* str, size_t len, char needle) {
    while (len) {
        if (str[len] == needle) {
            return len;
        }
        --len;
    }
    return -1;
}

static int parse_filename(const char* filename, struct filename_parse_result* result) {
    // https://uapi-group.org/specifications/specs/boot_loader_specification/#boot-counting
    int len, plus_position, minus_position;
    char buf[100];
    if (strncasecmp(filename, "fitImage_", 9)) {
        return -1;
    }
    len = strlen(filename);
    plus_position = str_rfind(filename, len, '+');
    if (plus_position < 0) {
        result->tries_total = -1;
        result->tries_left = -1;
        strcpy(result->tryless_path, filename);
    } else {
        strncpy(result->tryless_path, filename, plus_position);
        minus_position = str_rfind(filename, len, '-');
        if (minus_position > plus_position) {
            strncpy(buf, &filename[plus_position + 1], minus_position - plus_position - 1);
            result->tries_left = dectoul(buf, NULL);
            strncpy(buf, &filename[minus_position + 1], len - minus_position - 1);
            result->tries_total = dectoul(buf, NULL);
        } else {
            strncpy(buf, &filename[plus_position + 1], len - plus_position - 1);
            result->tries_total = dectoul(buf, NULL);
            result->tries_left = result->tries_total;
        }
    }
    return 0;
}

static bool is_char_meaningful(char c) {
    switch (c) {
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '0' ... '9':
        case '.': case '~':
        case '-': case '^':
            return true;
        default:
            return false;
    }
}

#define CHECK_LAST_CHAR(C) \
    if (*a == (C) && *b == (C)) { ++a; ++b; continue; } \
    else if (*a == (C) && *b != (C)) { return false; } \
    else if (*a != (C) && *b == (C)) { return true; }

static bool is_version_higher(const char* a, const char* b) {
    // https://uapi-group.org/specifications/specs/version_format_specification/
    ulong an, bn;
    char* ap, *bp;
    while (true) {
        // Step 1: Skip non-meaningful characters
        while (!is_char_meaningful(*a)) ++a;
        while (!is_char_meaningful(*b)) ++b;
        // Step 2: Tilde
        CHECK_LAST_CHAR('~');
        // Step 3: String length
        CHECK_LAST_CHAR('\0');
        // Step 4: Minus
        CHECK_LAST_CHAR('-');
        // Step 5: Caret
        CHECK_LAST_CHAR('^');
        // Step 6: Dot
        CHECK_LAST_CHAR('.');
        // Step 7: Numbers
        if (isdigit(*a) || isdigit(*b)) {
            an = dectoul(a, &ap);
            bn = dectoul(b, &bp);
            a = ap;
            b = bp;
            if (an < bn) return false;
            if (an > bn) return true;
            continue;
        }
        // Step 8: Letters
        while (isalpha(*a) && isalpha(*b)) {
            if (*a < *b) return false;
            if (*a > *b) return true;
            ++a; ++b;
        }
        if (isalpha(*a) && !isalpha(*b)) return true;
        if (!isalpha(*a) && isalpha(*b)) return false;
    }
    return true;
}

static int fit_blst2_read_bootflow(struct udevice *dev, struct bootflow *bflow)
{
    struct blk_desc *desc;
    const char *const *prefixes;
    struct udevice *bootstd;
    const char *prefix;
    char buf[256];
    int ret, i;
    struct filename_parse_result current_result;
    struct filename_parse_result best_result;
    char *best_image = NULL;
    ulong size;
    ulong loadaddr = env_get_hex("ramdisk_addr_r", 0);
    struct fs_dir_stream* dirls;
    struct fs_dirent* dirent;

    ret = uclass_first_device_err(UCLASS_BOOTSTD, &bootstd);
    if (ret)
        return log_msg_ret("std", ret);

    /* If a block device, we require a partition table */
    if (bflow->blk && !bflow->part)
        return -ENOENT;

    prefixes = bootstd_get_prefixes(bootstd);
    i = 0;
    desc = bflow->blk ? dev_get_uclass_plat(bflow->blk) : NULL;
    for (i=0; prefixes[i]; ++i) {
        prefix = prefixes ? prefixes[i] : NULL;
        ret = bootmeth_setup_fs(bflow, desc);
        if (ret) {
            return log_msg_ret("setup", ret);
        }
        printf("Checking %s...\n", prefixes[i]);
        dirls = fs_opendir(prefixes[i]);
        if (!dirls) {
            continue;
        }
        while ((dirent = fs_readdir(dirls))) {
            snprintf(buf, sizeof(buf), "%s%s", prefix, dirent->name);
            printf("Examining %s...\n", buf);
            if (!parse_filename(dirent->name, &current_result)) {
                printf("Left=%d total=%d\n", current_result.tries_left, current_result.tries_total);
                // We use the new image if the version is higher and (it hasn't failed or the current image has also failed)
                if (best_image && !(is_version_higher(buf, best_image) && (current_result.tries_left != 0 || best_result.tries_left == 0))) {
                    continue;
                }
                if (best_image) {
                    free(best_image);
                }
                printf("Found better image %s\n", buf);
                best_image = strdup(buf);
                best_result.tries_left = current_result.tries_left;
                best_result.tries_total = current_result.tries_total;
                strcpy(best_result.tryless_path, current_result.tryless_path);
            }
        }
        fs_closedir(dirls);
    }
    if (!best_image) {
        return log_msg_ret("noimage", -ENOENT);
    }
    bflow->fname = best_image;
    if (best_result.tries_left > 0) {
        snprintf(buf, sizeof(buf), "%s+%d-%d", best_result.tryless_path, best_result.tries_left - 1, best_result.tries_total);
        printf("Renaming to %s\n", buf);
        ret = bootmeth_setup_fs(bflow, desc);
        if (ret) {
            return log_msg_ret("setup", ret);
        }
        ret = fs_rename(best_image, buf);
        if (ret) {
            return log_msg_ret("rename", ret);
        }
        strcpy(best_image, buf);
    }

    size = SZ_1G;
    ret = bootmeth_read_file(dev, bflow, bflow->fname, loadaddr, &size);
    if (ret)
        return log_msg_ret("read", ret);

    bflow->buf = map_sysmem(loadaddr, 0);
    bflow->state = BOOTFLOWST_READY;
    if (env_get("bootargs")) {
        bflow->cmdline = strdup(env_get("bootargs"));
    }
    snprintf(buf, sizeof(buf), "PARTLABEL=rootfs_%s", &best_result.tryless_path[9]);
    bootflow_cmdline_set_arg(bflow, "root", buf, false);

    return 0;
}

static int fit_blst2_boot(struct udevice *dev, struct bootflow *bflow)
{
    int ret;
    char buf[256];
    char* console;
    char* baudrate;
    struct bootm_info bmi;

    bootflow_cmdline_set_arg(bflow, "ro", BOOTFLOWCL_EMPTY, false);
    bootflow_cmdline_set_arg(bflow, "rootwait", BOOTFLOWCL_EMPTY, false);

    console = env_get("console");
    if (!console) {
        return log_msg_ret("console", -ENOENT);
    }
    baudrate = env_get("baudrate");
    if (!baudrate) {
        return log_msg_ret("baudrate", -ENOENT);
    }
    snprintf(buf, sizeof(buf), "%s,%s", console, baudrate);
    bootflow_cmdline_set_arg(bflow, "console", buf, true);

    snprintf(buf, sizeof(buf), "%lx#conf-%s", (ulong)map_to_sysmem(bflow->buf), env_get("fdtfile"));
    printf("Running (equiv of): bootm %s\n", buf);
    printf("Kernel cmdline: %s\n", env_get("bootargs"));

    bootm_init(&bmi);
    bmi.addr_img = buf;
    ret = bootm_run(&bmi);

    return log_msg_ret("go", ret);
}

static int fit_blst2_bootmeth_bind(struct udevice *dev)
{
    struct bootmeth_uc_plat *plat = dev_get_uclass_plat(dev);

    plat->desc = IS_ENABLED(CONFIG_BOOTSTD_FULL) ?
        "FIT BLS type #2 boot from a block device" : "fit_blst2";

    return 0;
}

static struct bootmeth_ops fit_blst2_bootmeth_ops = {
    .check		= fit_blst2_check,
    .read_bootflow	= fit_blst2_read_bootflow,
    .read_file	= bootmeth_common_read_file,
    .boot		= fit_blst2_boot,
};

static const struct udevice_id fit_blst2_bootmeth_ids[] = {
    { .compatible = "u-boot,fit_blst2" },
    { }
};

/* Put an number before 'fit_blst2' to provide a default ordering */
U_BOOT_DRIVER(bootmeth_0fit_blst2) = {
    .name		= "bootmeth_fit_blst2",
    .id		= UCLASS_BOOTMETH,
    .of_match	= fit_blst2_bootmeth_ids,
    .ops		= &fit_blst2_bootmeth_ops,
    .bind		= fit_blst2_bootmeth_bind,
};
