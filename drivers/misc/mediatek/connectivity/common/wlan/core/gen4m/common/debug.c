/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
#include "precomp.h"

uint32_t wlanSetDriverDbgLevel(IN uint32_t u4DbgIdx, IN uint32_t u4DbgMask)
{
	uint32_t u4Idx;
	uint32_t fgStatus = WLAN_STATUS_SUCCESS;

	if (u4DbgIdx == DBG_ALL_MODULE_IDX) {
		for (u4Idx = 0; u4Idx < DBG_MODULE_NUM; u4Idx++)
			aucDebugModule[u4Idx] = (uint8_t) u4DbgMask;
		LOG_FUNC("Set ALL DBG module log level to [0x%02x]\n",
				u4DbgMask);
	} else if (u4DbgIdx < DBG_MODULE_NUM) {
		aucDebugModule[u4DbgIdx] = (uint8_t) u4DbgMask;
		LOG_FUNC("Set DBG module[%u] log level to [0x%02x]\n",
				u4DbgIdx, u4DbgMask);
	} else {
		fgStatus = WLAN_STATUS_FAILURE;
	}

	if (fgStatus == WLAN_STATUS_SUCCESS)
		wlanDriverDbgLevelSync();

	return fgStatus;
}

uint32_t wlanGetDriverDbgLevel(IN uint32_t u4DbgIdx, OUT uint32_t *pu4DbgMask)
{
	if (u4DbgIdx < DBG_MODULE_NUM) {
		*pu4DbgMask = aucDebugModule[u4DbgIdx];
		return WLAN_STATUS_SUCCESS;
	}

	return WLAN_STATUS_FAILURE;
}

uint32_t wlanDbgLevelUiSupport(IN struct ADAPTER *prAdapter, uint32_t u4Version,
		uint32_t ucModule)
{
	uint32_t u4Enable = ENUM_WIFI_LOG_LEVEL_SUPPORT_DISABLE;

	switch (u4Version) {
	case ENUM_WIFI_LOG_LEVEL_VERSION_V1:
		switch (ucModule) {
		case ENUM_WIFI_LOG_MODULE_DRIVER:
			u4Enable = ENUM_WIFI_LOG_LEVEL_SUPPORT_ENABLE;
			break;
		case ENUM_WIFI_LOG_MODULE_FW:
			u4Enable = ENUM_WIFI_LOG_LEVEL_SUPPORT_ENABLE;
			break;
		}
		break;
	default:
		break;
	}

	return u4Enable;
}

uint32_t wlanDbgGetLogLevelImpl(IN struct ADAPTER *prAdapter,
		uint32_t u4Version, uint32_t ucModule)
{
	uint32_t u4Level = ENUM_WIFI_LOG_LEVEL_DEFAULT;

	switch (u4Version) {
	case ENUM_WIFI_LOG_LEVEL_VERSION_V1:
		wlanDbgGetGlobalLogLevel(ucModule, &u4Level);
		break;
	default:
		break;
	}

	return u4Level;
}

void wlanDbgSetLogLevelImpl(IN struct ADAPTER *prAdapter,
		uint32_t u4Version, uint32_t u4Module, uint32_t u4level)
{
	uint32_t u4DriverLevel = ENUM_WIFI_LOG_LEVEL_DEFAULT;
	uint32_t u4FwLevel = ENUM_WIFI_LOG_LEVEL_DEFAULT;

	if (u4level >= ENUM_WIFI_LOG_LEVEL_NUM)
		return;

	switch (u4Version) {
	case ENUM_WIFI_LOG_LEVEL_VERSION_V1:
		wlanDbgSetGlobalLogLevel(u4Module, u4level);
		switch (u4Module) {
		case ENUM_WIFI_LOG_MODULE_DRIVER:
		{
			uint32_t u4DriverLogMask;

			if (u4level == ENUM_WIFI_LOG_LEVEL_DEFAULT)
				u4DriverLogMask = DBG_LOG_LEVEL_DEFAULT;
			else if (u4level == ENUM_WIFI_LOG_LEVEL_MORE)
				u4DriverLogMask = DBG_LOG_LEVEL_MORE;
			else
				u4DriverLogMask = DBG_LOG_LEVEL_EXTREME;

			wlanSetDriverDbgLevel(DBG_ALL_MODULE_IDX,
					(u4DriverLogMask & DBG_CLASS_MASK));
		}
			break;
		case ENUM_WIFI_LOG_MODULE_FW:
		{
			struct CMD_EVENT_LOG_UI_INFO cmd;

			kalMemZero(&cmd,
					sizeof(struct CMD_EVENT_LOG_UI_INFO));
			cmd.u4Version = u4Version;
			cmd.u4LogLevel = u4level;

			wlanSendSetQueryCmd(prAdapter,
					CMD_ID_LOG_UI_INFO,
					TRUE,
					FALSE,
					FALSE,
					nicCmdEventSetCommon,
					nicOidCmdTimeoutCommon,
					sizeof(struct CMD_EVENT_LOG_UI_INFO),
					(uint8_t *)&cmd,
					NULL,
					0);
		}
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	wlanDbgGetGlobalLogLevel(ENUM_WIFI_LOG_MODULE_DRIVER, &u4DriverLevel);
	wlanDbgGetGlobalLogLevel(ENUM_WIFI_LOG_MODULE_FW, &u4FwLevel);
#if (CFG_BUILT_IN_DRIVER == 0) && (CFG_MTK_ANDROID_WMT == 1)
	/*
	 * The function definition of get_logtoomuch_enable() and
	 * set_logtoomuch_enable of Android O0 or lower version are different
	 * from that of Android O1 or higher version. Wlan driver supports .ko
	 * module from Android O1. Use CFG_BUILT_IN_DRIVER to distinguish
	 * Android version higher than O1 instead.
	 */
	if ((u4DriverLevel > ENUM_WIFI_LOG_LEVEL_DEFAULT ||
			u4FwLevel > ENUM_WIFI_LOG_LEVEL_DEFAULT) &&
			get_logtoomuch_enable()) {
		DBGLOG(OID, TRACE,
			"Disable printk to much. driver: %d, fw: %d\n",
			u4DriverLevel,
			u4FwLevel);
		set_logtoomuch_enable(0);
	}
#endif
}

u_int8_t wlanDbgGetGlobalLogLevel(uint32_t u4Module, uint32_t *pu4Level)
{
	if (u4Module != ENUM_WIFI_LOG_MODULE_DRIVER &&
			u4Module != ENUM_WIFI_LOG_MODULE_FW)
		return FALSE;

	*pu4Level = au4LogLevel[u4Module];
	return TRUE;
}

u_int8_t wlanDbgSetGlobalLogLevel(uint32_t u4Module, uint32_t u4Level)
{
	if (u4Module != ENUM_WIFI_LOG_MODULE_DRIVER &&
			u4Module != ENUM_WIFI_LOG_MODULE_FW)
		return FALSE;

	au4LogLevel[u4Module] = u4Level;
	return TRUE;
}

void wlanDriverDbgLevelSync(void)
{
	uint8_t i = 0;
	uint32_t u4Mask = DBG_CLASS_MASK;
	uint32_t u4DriverLogLevel = ENUM_WIFI_LOG_LEVEL_DEFAULT;

	/* get the lowest level as module's level */
	for (i = 0; i < DBG_MODULE_NUM; i++)
		u4Mask &= aucDebugModule[i];

	if ((u4Mask & DBG_LOG_LEVEL_EXTREME) == DBG_LOG_LEVEL_EXTREME)
		u4DriverLogLevel = ENUM_WIFI_LOG_LEVEL_EXTREME;
	else if ((u4Mask & DBG_LOG_LEVEL_MORE) == DBG_LOG_LEVEL_MORE)
		u4DriverLogLevel = ENUM_WIFI_LOG_LEVEL_MORE;
	else
		u4DriverLogLevel = ENUM_WIFI_LOG_LEVEL_DEFAULT;

	wlanDbgSetGlobalLogLevel(ENUM_WIFI_LOG_MODULE_DRIVER, u4DriverLogLevel);
}

static void
firmwareHexDump(const uint8_t *pucPreFix,
		int32_t i4PreFixType,
		int32_t i4RowSize, int32_t i4GroupSize,
		const void *pvBuf, size_t len, u_int8_t fgAscii)
{
#define OLD_KBUILD_MODNAME KBUILD_MODNAME
#undef KBUILD_MODNAME
#define KBUILD_MODNAME "wlan_mt6632_fw"

	const uint8_t *pucPtr = pvBuf;
	int32_t i, i4LineLen, i4Remaining = len;
	uint8_t ucLineBuf[32 * 3 + 2 + 32 + 1];

	if (i4RowSize != 16 && i4RowSize != 32)
		i4RowSize = 16;

	for (i = 0; i < len; i += i4RowSize) {
		i4LineLen = min(i4Remaining, i4RowSize);
		i4Remaining -= i4RowSize;

		/* use kernel API */
		hex_dump_to_buffer(pucPtr + i, i4LineLen, i4RowSize,
				   i4GroupSize,
				   ucLineBuf, sizeof(ucLineBuf), fgAscii);

		switch (i4PreFixType) {
		case DUMP_PREFIX_ADDRESS:
			pr_no_info("%s%p: %s\n",
				pucPreFix, pucPtr + i, ucLineBuf);
			break;
		case DUMP_PREFIX_OFFSET:
			pr_no_info("%s%.8x: %s\n", pucPreFix, i, ucLineBuf);
			break;
		default:
			pr_no_info("%s%s\n", pucPreFix, ucLineBuf);
			break;
		}
	}
#undef KBUILD_MODNAME
#define KBUILD_MODNAME OLD_KBUILD_MODNAME
}

void wlanPrintFwLog(uint8_t *pucLogContent,
		    uint16_t u2MsgSize, uint8_t ucMsgType,
		    const uint8_t *pucFmt, ...)
{
#define OLD_KBUILD_MODNAME KBUILD_MODNAME
#define OLD_LOG_FUNC LOG_FUNC
#undef KBUILD_MODNAME
#undef LOG_FUNC
#define KBUILD_MODNAME "wlan_mt6632_fw"
#define LOG_FUNC pr_no_info
#define DBG_LOG_BUF_SIZE 128

	int8_t aucLogBuffer[DBG_LOG_BUF_SIZE];
	va_list args;

	if (u2MsgSize > DEBUG_MSG_SIZE_MAX - 1) {
		LOG_FUNC("Firmware Log Size(%d) is too large, type %d\n",
			 u2MsgSize, ucMsgType);
		return;
	}
	switch (ucMsgType) {
	case DEBUG_MSG_TYPE_ASCII: {
		uint8_t *pucChr;

		pucLogContent[u2MsgSize] = '\0';

		/* skip newline */
		pucChr = kalStrChr(pucLogContent, '\0');
		if (*(pucChr - 1) == '\n')
			*(pucChr - 1) = '\0';

		LOG_FUNC("<FW>%s\n", pucLogContent);
	}
	break;
	case DEBUG_MSG_TYPE_DRIVER:
		/* Only 128 Bytes is available to print in driver */
		va_start(args, pucFmt);
		vsnprintf(aucLogBuffer, sizeof(aucLogBuffer) - 1, pucFmt,
			  args);
		va_end(args);
		aucLogBuffer[DBG_LOG_BUF_SIZE - 1] = '\0';
		LOG_FUNC("%s\n", aucLogBuffer);
		break;
	case DEBUG_MSG_TYPE_MEM8:
		firmwareHexDump("fw data:", DUMP_PREFIX_ADDRESS,
				16, 1, pucLogContent, u2MsgSize, true);
		break;
	default:
		firmwareHexDump("fw data:", DUMP_PREFIX_ADDRESS,
				16, 4, pucLogContent, u2MsgSize, true);
		break;
	}

#undef KBUILD_MODNAME
#undef LOG_FUNC
#define KBUILD_MODNAME OLD_KBUILD_MODNAME
#define LOG_FUNC OLD_LOG_FUNC
#undef OLD_KBUILD_MODNAME
#undef OLD_LOG_FUNC
}

/* Begin: Functions used to breakdown packet jitter, for test case VoE 5.7 */
static void wlanSetBE32(uint32_t u4Val, uint8_t *pucBuf)
{
	uint8_t *littleEn = (uint8_t *)&u4Val;

	pucBuf[0] = littleEn[3];
	pucBuf[1] = littleEn[2];
	pucBuf[2] = littleEn[1];
	pucBuf[3] = littleEn[0];
}

void wlanFillTimestamp(struct ADAPTER *prAdapter, void *pvPacket,
		       uint8_t ucPhase)
{
	struct sk_buff *skb = (struct sk_buff *)pvPacket;
	uint8_t *pucEth = NULL;
	uint32_t u4Length = 0;
	uint8_t *pucUdp = NULL;
	struct timeval tval;

	if (!prAdapter || !prAdapter->rDebugInfo.fgVoE5_7Test || !skb)
		return;
	pucEth = skb->data;
	u4Length = skb->len;
	if (u4Length < 200 ||
	    ((pucEth[ETH_TYPE_LEN_OFFSET] << 8) |
	     (pucEth[ETH_TYPE_LEN_OFFSET + 1])) != ETH_P_IPV4)
		return;
	if (pucEth[ETH_HLEN+9] != IP_PRO_UDP)
		return;
	pucUdp = &pucEth[ETH_HLEN+28];
	if (kalStrnCmp(pucUdp, "1345678", 7))
		return;
	do_gettimeofday(&tval);
	switch (ucPhase) {
	case PHASE_XMIT_RCV: /* xmit */
		pucUdp += 20;
		break;
	case PHASE_ENQ_QM: /* enq */
		pucUdp += 28;
		break;
	case PHASE_HIF_TX: /* tx */
		pucUdp += 36;
		break;
	}
	wlanSetBE32(tval.tv_sec, pucUdp);
	wlanSetBE32(tval.tv_usec, pucUdp+4);
}
/* End: Functions used to breakdown packet jitter, for test case VoE 5.7 */
