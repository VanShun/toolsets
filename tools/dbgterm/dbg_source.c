/**
 * @file    dbg_source.c
 * @author  herongshun
 * @date    2026-11-06
 * @brief   调试终端数据源接口层实现(环形缓冲/节流通知/实现分发)
 */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "dbg_source.h"
#include "adb_core.h"

/* -------------------------------------------------------------------------- */
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   环形缓冲通知(节流): 距上次通知不足节流间隔则忽略
 * @param   p_rx 环形缓冲对象
 * @return  无
 */
static void dbg_source_notify_throttled(DBG_SOURCE_RX_T *p_rx)
{
    uint32_t now = GetTickCount();

    if ((now - p_rx->last_notify_tick) < DBG_SOURCE_THROTTLE_MS) {
        return;
    }
    p_rx->last_notify_tick = now;
    (void)PostMessageW(p_rx->h_notify, p_rx->notify_msg, 0u, 0u);
}

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

void dbg_source_rx_init(DBG_SOURCE_RX_T *p_rx, HWND h_notify, uint32_t notify_msg)
{
    if (p_rx == NULL) {
        return;
    }
    memset(p_rx, 0, sizeof(DBG_SOURCE_RX_T));
    InitializeCriticalSection(&p_rx->cs);
    p_rx->h_notify = h_notify;
    p_rx->notify_msg = notify_msg;
    p_rx->last_notify_tick = GetTickCount();
}

void dbg_source_rx_push(DBG_SOURCE_RX_T *p_rx, const uint8_t *p_data, uint32_t len, uint8_t notify)
{
    uint32_t free_bytes = 0u;
    uint32_t copy_len = 0u;
    uint32_t first_seg = 0u;

    if ((p_rx == NULL) || (p_data == NULL) || (len == 0u)) {
        return;
    }

    EnterCriticalSection(&p_rx->cs);
    free_bytes = DBG_SOURCE_RX_BUF_SIZE - p_rx->used;
    copy_len = (len <= free_bytes) ? len : free_bytes;

    if (copy_len > 0u) {
        first_seg = DBG_SOURCE_RX_BUF_SIZE - p_rx->wr;
        if (first_seg > copy_len) {
            first_seg = copy_len;
        }
        memcpy(&p_rx->buf[p_rx->wr], p_data, first_seg);
        if (copy_len > first_seg) {
            memcpy(&p_rx->buf[0], &p_data[first_seg], (copy_len - first_seg));
        }
        p_rx->wr = (p_rx->wr + copy_len) % DBG_SOURCE_RX_BUF_SIZE;
        p_rx->used += copy_len;
    }
    p_rx->overflow += (len - copy_len);

    if (notify != DBG_SOURCE_NOTIFY_NONE) {
        dbg_source_notify_throttled(p_rx);
    }
    LeaveCriticalSection(&p_rx->cs);
}

uint32_t dbg_source_rx_drain(DBG_SOURCE_RX_T *p_rx, uint8_t *p_out, uint32_t max_bytes)
{
    uint32_t take_len = 0u;
    uint32_t first_seg = 0u;

    if ((p_rx == NULL) || (p_out == NULL) || (max_bytes == 0u)) {
        return 0u;
    }

    EnterCriticalSection(&p_rx->cs);
    take_len = (p_rx->used <= max_bytes) ? p_rx->used : max_bytes;
    if (take_len > 0u) {
        first_seg = DBG_SOURCE_RX_BUF_SIZE - p_rx->rd;
        if (first_seg > take_len) {
            first_seg = take_len;
        }
        memcpy(p_out, &p_rx->buf[p_rx->rd], first_seg);
        if (take_len > first_seg) {
            memcpy(&p_out[first_seg], &p_rx->buf[0], (take_len - first_seg));
        }
        p_rx->rd = (p_rx->rd + take_len) % DBG_SOURCE_RX_BUF_SIZE;
        p_rx->used -= take_len;
    }
    LeaveCriticalSection(&p_rx->cs);

    return take_len;
}

uint32_t dbg_source_rx_take_overflow(DBG_SOURCE_RX_T *p_rx)
{
    uint32_t overflow = 0u;

    if (p_rx == NULL) {
        return 0u;
    }
    EnterCriticalSection(&p_rx->cs);
    overflow = p_rx->overflow;
    p_rx->overflow = 0u;
    LeaveCriticalSection(&p_rx->cs);

    return overflow;
}

uint8_t dbg_source_rx_wait_mark(DBG_SOURCE_RX_T *p_rx, const char *p_needle, uint32_t timeout_ms)
{
    uint32_t start_tick = GetTickCount();
    uint32_t start_wr = 0u;
    uint32_t nlen = 0u;
    uint32_t i = 0u;

    if ((p_rx == NULL) || (p_needle == NULL) || (p_needle[0] == '\0')) {
        return 0u;
    }
    nlen = (uint32_t)strlen(p_needle);

    EnterCriticalSection(&p_rx->cs);
    start_wr = p_rx->wr;
    LeaveCriticalSection(&p_rx->cs);

    for (;;) {
        uint8_t found = 0u;
        uint32_t count = 0u;

        EnterCriticalSection(&p_rx->cs);
        count = (p_rx->wr + DBG_SOURCE_RX_BUF_SIZE - start_wr) % DBG_SOURCE_RX_BUF_SIZE;
        for (i = 0u; ((found == 0u) && ((i + nlen) <= count)); i++) {
            uint32_t j = 0u;
            uint8_t match = 1u;

            for (j = 0u; j < nlen; j++) {
                uint8_t c = p_rx->buf[(start_wr + i + j) % DBG_SOURCE_RX_BUF_SIZE];
                uint8_t d = (uint8_t)p_needle[j];

                if ((c >= (uint8_t)'A') && (c <= (uint8_t)'Z')) {
                    c = (uint8_t)(c + ((uint8_t)'a' - (uint8_t)'A'));   /* 大小写不敏感 */
                }
                if (c != d) {
                    match = 0u;
                    break;
                }
            }
            if (match != 0u) {
                found = 1u;
            }
        }
        LeaveCriticalSection(&p_rx->cs);

        if (found != 0u) {
            return 1u;
        }
        if ((GetTickCount() - start_tick) >= timeout_ms) {
            return 0u;
        }
        Sleep(30u);
    }
}

const DBG_SOURCE_T *dbg_source_get(uint32_t type)
{
    const DBG_SOURCE_T *p_source = NULL;

    switch (type)
    {
        case DBG_SRC_ADB:
        {
            p_source = adb_core_get_source();
            break;
        }
        case DBG_SRC_SERIAL:
        {
            p_source = NULL;   /* 串口数据源: 第二阶段实现 */
            break;
        }
        default:
        {
            p_source = NULL;
            break;
        }
    }

    return p_source;
}
