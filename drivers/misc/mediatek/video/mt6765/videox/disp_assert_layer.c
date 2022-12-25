/*
 * Copyright (C) 2015 MediaTek Inc.
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

#include <linux/types.h>
#include "primary_display.h"
#include "ddp_hal.h"
#include "disp_drv_log.h"
#include "disp_assert_layer.h"
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include "ddp_mmp.h"
#include "disp_drv_platform.h"
#include "disp_session.h"
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/semaphore.h>
#include <asm/cacheflush.h>
#include <linux/module.h>

/* /common part */
#define DAL_BPP             (2)
#define DAL_WIDTH           (DISP_GetScreenWidth())
#define DAL_HEIGHT          (DISP_GetScreenHeight())

#ifdef CONFIG_MTK_FB_SUPPORT_ASSERTION_LAYER
/* #if defined(CONFIG_MTK_FB_SUPPORT_ASSERTION_LAYER) */

#include "mtkfb_console.h"

/* ------------------------------------------------------------------------- */
#define DAL_FORMAT          (DISP_FORMAT_RGB565)
#define DAL_BG_COLOR        (dal_bg_color)
#define DAL_FG_COLOR        (dal_fg_color)

#define RGB888_To_RGB565(x) ((((x) & 0xF80000) >> 8) |                      \
			     (((x) & 0x00FC00) >> 5) |                      \
			     (((x) & 0x0000F8) >> 3))

#define MAKE_TWO_RGB565_COLOR(high, low)  (((low) << 16) | (high))

DEFINE_SEMAPHORE(dal_sem);

inline enum DAL_STATUS DAL_LOCK(void)
{
	if (down_interruptible(&dal_sem)) {
		DISP_LOG_PRINT(ANDROID_LOG_WARN, "DAL",
			"Can't get semaphore in %s()\n", __func__);
		return DAL_STATUS_LOCK_FAIL;
	}
	return DAL_STATUS_OK;
}

#define DAL_UNLOCK() up(&dal_sem)

inline enum MFC_STATUS DAL_CHECK_MFC_RET(enum MFC_STATUS expr)
{
	enum MFC_STATUS ret = expr;

	if (ret != MFC_STATUS_OK) {
		DISP_LOG_PRINT(ANDROID_LOG_WARN, "DAL",
			       "Warning: call MFC_XXX function failed in %s(), line: %d, ret: %d\n",
			       __func__, __LINE__, ret);
		return ret;
	}
	return MFC_STATUS_OK;
}


inline enum DISP_STATUS DAL_CHECK_DISP_RET(enum DISP_STATUS expr)
{
	enum DISP_STATUS ret = (expr);

	if (ret != DISP_STATUS_OK) {
		DISP_LOG_PRINT(ANDROID_LOG_WARN, "DAL",
			       "Warning: call DISP_XXX function failed in %s(), line: %d, ret: %d\n",
			       __func__, __LINE__, ret);
		return ret;
	}
	return DISP_STATUS_OK;
}

#define DAL_LOG(fmt, arg...)
/* ------------------------------------------------------------------------- */

static MFC_HANDLE mfc_handle;
static void *dal_fb_addr;
static unsigned long dal_fb_pa;

/*static BOOL dal_enable_when_resume = FALSE;*/
static bool dal_disable_when_resume;
static unsigned int dal_fg_color = RGB888_To_RGB565(DAL_COLOR_WHITE);
static unsigned int dal_bg_color = RGB888_To_RGB565(DAL_COLOR_RED);
static char dal_print_buffer[1024];

bool dal_shown;
unsigned int isAEEEnabled;
unsigned int dump_output;
unsigned int dump_output_comp;
void *composed_buf;
/* ------------------------------------------------------------------------- */


uint32_t DAL_GetLayerSize(void)
{
	/* avoid lcdc read buffersize+1 issue */
	return DAL_WIDTH * DAL_HEIGHT * DAL_BPP + 4096;
}

enum DAL_STATUS DAL_SetScreenColor(enum DAL_COLOR color)
{
	return DAL_STATUS_OK;
}
EXPORT_SYMBOL(DAL_SetScreenColor);

enum DAL_STATUS DAL_Init(unsigned long layerVA, unsigned long layerPA)
{
	return DAL_STATUS_OK;
}

enum DAL_STATUS DAL_SetColor(unsigned int fgColor, unsigned int bgColor)
{
	return DAL_STATUS_OK;
}
EXPORT_SYMBOL(DAL_SetColor);

enum DAL_STATUS DAL_Dynamic_Change_FB_Layer(unsigned int isAEEEnabled)
{
	return DAL_STATUS_OK;
}

static int show_dal_layer(int enable)
{
	return DAL_STATUS_OK;
}

enum DAL_STATUS DAL_Clean(void)
{
	return DAL_STATUS_OK;
}
EXPORT_SYMBOL(DAL_Clean);

int is_DAL_Enabled(void)
{
	int ret = 0;

	ret = isAEEEnabled;
	return ret;
}

enum DAL_STATUS DAL_Printf(const char *fmt, ...)
{
	return DAL_STATUS_OK;
}
EXPORT_SYMBOL(DAL_Printf);

enum DAL_STATUS DAL_OnDispPowerOn(void)
{
	return DAL_STATUS_OK;
}

void *show_layers_va;
MFC_HANDLE show_mfc_handle;
enum DAL_COLOR color_wdma[24] = {
	DAL_COLOR_PINK,
	DAL_COLOR_GREEN,
	DAL_COLOR_BLUE,
	DAL_COLOR_RED,
	DAL_COLOR_MAROON,
	DAL_COLOR_STEEL_BLUE,
	DAL_COLOR_DARK_CYAN,
	DAL_COLOR_OLIVE_GREEN,
	DAL_COLOR_CORNSILK,
	DAL_COLOR_TURQUOISE,
	DAL_COLOR_YELLOW,
	DAL_COLOR_BLACK,
	};

static const char *digit[24] = {
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
};

int show_layers_draw_wdma(struct Layer_draw_info *info)
{
	int i, j, k;
	int layer_num;
	int width_x, height_y;
	void *buf_pos;

	if (show_layers_va == NULL || show_mfc_handle == NULL)
		return -1;

	layer_num = info->layer_num;
	for (i = layer_num - 1; i >= 0; i--) {
		width_x = info->bot_r_x - info->top_l_x;
		height_y = info->bot_r_y - info->top_l_y;
		for (j = info->top_l_y[i];
		j < info->top_l_y[i] + info->frame_width[i]; j++) {
			buf_pos = show_layers_va + j * DAL_WIDTH * 3;
			for (k = info->top_l_x[i]; k <= info->bot_r_x[i]; k++) {
				*(char *)(buf_pos + 3 * k + 2) =
					(color_wdma[i] & 0xff0000) >> 16;
				*(char *)(buf_pos + 3 * k + 1) =
					(color_wdma[i] & 0x00ff00) >> 8;
				*(char *)(buf_pos + 3 * k) =
					color_wdma[i] & 0x0000ff;
			}
		}
		for (j = info->bot_r_y[i] - info->frame_width[i] + 1;
		j < info->bot_r_y[i] + 1; j++) {
			buf_pos = (void *)(show_layers_va + j * DAL_WIDTH * 3);
			for (k = info->top_l_x[i]; k <= info->bot_r_x[i]; k++) {
				*(char *)(buf_pos + 3 * k + 2) =
					(color_wdma[i] & 0xff0000) >> 16;
				*(char *)(buf_pos + 3 * k + 1) =
					(color_wdma[i] & 0x00ff00) >> 8;
				*(char *)(buf_pos + 3 * k) =
					color_wdma[i] & 0x0000ff;
			}
		}
		for (j = info->top_l_y[i]; j < info->bot_r_y[i]; j++) {
			buf_pos = show_layers_va + j * DAL_WIDTH * 3;
			for (k = info->top_l_x[i];
			k < info->top_l_x[i] + info->frame_width[i]; k++) {
				*(char *)(buf_pos + 3 * k + 2) =
					(color_wdma[i] & 0xff0000) >> 16;
				*(char *)(buf_pos + 3 * k + 1) =
					(color_wdma[i] & 0x00ff00) >> 8;
				*(char *)(buf_pos + 3 * k) =
					color_wdma[i] & 0x0000ff;
			}
			for (k = info->bot_r_x[i] - info->frame_width[i] + 1;
			k < info->bot_r_x[i] + 1; k++) {
				*(char *)(buf_pos + 3 * k + 2) =
					(color_wdma[i] & 0xff0000) >> 16;
				*(char *)(buf_pos + 3 * k + 1) =
					(color_wdma[i] & 0x00ff00) >> 8;
				*(char *)(buf_pos + 3 * k) =
					color_wdma[i] & 0x0000ff;
			}
		}
	}
	for (i = 0; i < layer_num; i++) {
		MFC_SetCursor(show_mfc_handle, info->dx[i]/8,
			info->dy[i]/16);
		MFC_SetColor(show_mfc_handle, color_wdma[i], DAL_COLOR_WHITE);
		MFC_Print(show_mfc_handle, digit[i]);
	}
	return 0;
}

/* ########################################################################## */
/* !CONFIG_MTK_FB_SUPPORT_ASSERTION_LAYER */
/* ########################################################################## */
#else
unsigned int isAEEEnabled;

uint32_t DAL_GetLayerSize(void)
{
	/* xuecheng, avoid lcdc read buffersize+1 issue */
	return DAL_WIDTH * DAL_HEIGHT * DAL_BPP + 4096;
}

enum DAL_STATUS DAL_Init(unsigned long layerVA, unsigned long layerPA)
{
	return DAL_STATUS_OK;
}

enum DAL_STATUS DAL_SetColor(unsigned int fgColor, unsigned int bgColor)
{
	return DAL_STATUS_OK;
}
EXPORT_SYMBOL(DAL_SetColor);

enum DAL_STATUS DAL_Clean(void)
{
	return DAL_STATUS_OK;
}
EXPORT_SYMBOL(DAL_Clean);

enum DAL_STATUS DAL_Printf(const char *fmt, ...)
{
	return DAL_STATUS_OK;
}
EXPORT_SYMBOL(DAL_Printf);

enum DAL_STATUS DAL_OnDispPowerOn(void)
{
	return DAL_STATUS_OK;
}

enum DAL_STATUS DAL_SetScreenColor(enum DAL_COLOR color)
{
	return DAL_STATUS_OK;
}
EXPORT_SYMBOL(DAL_SetScreenColor);

int is_DAL_Enabled(void)
{
	return 0;
}

#endif /* CONFIG_MTK_FB_SUPPORT_ASSERTION_LAYER */
