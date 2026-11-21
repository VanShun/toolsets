/**
 * @file    gb_records.c
 * @author  herongshun
 * @date    2026-11-21
 * @brief   GB32960 解析记录管理实现
 *          环形数组 + 逐条堆分配; 单线程UI调用, 非线程安全
 */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "gb32960.h"
#include "gb_records.h"

/* -------------------------------------------------------------------------- */
/*  内部类型                                                                    */
/* -------------------------------------------------------------------------- */

/** 单条记录 */
typedef struct GB_REC_TAG
{
    uint8_t  *p_data;    /**< 帧数据堆缓冲 */
    uint32_t  len;       /**< 帧长度(字节) */
} GB_REC_T;

/* -------------------------------------------------------------------------- */
/*  模块内部全局变量                                                           */
/* -------------------------------------------------------------------------- */

static GB_REC_T g_gb_recs[GB_REC_RECORD_MAX] = {0};   /**< 记录环形数组 */
static uint32_t g_gb_rec_count = 0u;                   /**< 当前记录数 */
static uint32_t g_gb_rec_head = 0u;                     /**< 最旧记录物理槽位 */

/* -------------------------------------------------------------------------- */
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   取环形数组逻辑槽位(索引0=最旧)
 * @param   idx 逻辑索引(0起)
 * @return  物理槽位
 */
static uint32_t rec_slot(uint32_t idx)
{
    return ((g_gb_rec_head + idx) % GB_REC_RECORD_MAX);
}

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

void gb_records_clear(void)
{
    uint32_t i = 0u;

    for (i = 0u; i < GB_REC_RECORD_MAX; i++) {
        if (g_gb_recs[i].p_data != NULL) {
            (void)HeapFree(GetProcessHeap(), 0u, g_gb_recs[i].p_data);
            g_gb_recs[i].p_data = NULL;
            g_gb_recs[i].len = 0u;
        }
    }
    g_gb_rec_count = 0u;
    g_gb_rec_head = 0u;
}

uint32_t gb_records_count(void)
{
    return g_gb_rec_count;
}

uint8_t gb_records_add(const uint8_t *p_frame, uint32_t len)
{
    uint8_t *p_copy = NULL;
    uint32_t slot = 0u;

    if ((p_frame == NULL) || (len == 0u) || (len > GB32960_FRAME_MAX)) {
        return 0u;
    }
    p_copy = (uint8_t *)HeapAlloc(GetProcessHeap(), 0u, (SIZE_T)len);
    if (p_copy == NULL) {
        return 0u;
    }
    memcpy(p_copy, p_frame, len);

    if (g_gb_rec_count >= GB_REC_RECORD_MAX) {
        /* 满员: 淘汰最旧 */
        (void)HeapFree(GetProcessHeap(), 0u, g_gb_recs[g_gb_rec_head].p_data);
        g_gb_recs[g_gb_rec_head].p_data = NULL;
        g_gb_rec_head = ((g_gb_rec_head + 1u) % GB_REC_RECORD_MAX);
        g_gb_rec_count--;
    }
    slot = rec_slot(g_gb_rec_count);
    g_gb_recs[slot].p_data = p_copy;
    g_gb_recs[slot].len = len;
    g_gb_rec_count++;

    return 1u;
}

const uint8_t *gb_records_get(uint32_t idx, uint32_t *p_len)
{
    const GB_REC_T *p_rec = NULL;

    if (idx >= g_gb_rec_count) {
        return NULL;
    }
    p_rec = &g_gb_recs[rec_slot(idx)];
    if (p_rec->p_data == NULL) {
        return NULL;
    }
    if (p_len != NULL) {
        *p_len = p_rec->len;
    }

    return p_rec->p_data;
}

void gb_records_summary(uint32_t idx, wchar_t *p_out, uint32_t cap)
{
    const uint8_t *p_frame = gb_records_get(idx, NULL);
    uint8_t cmd = 0u;
    uint32_t data_len = 0u;
    uint32_t i = 0u;

    if ((p_out == NULL) || (cap < 8u)) {
        return;
    }
    p_out[0] = L'\0';
    if (p_frame == NULL) {
        return;
    }
    cmd = p_frame[2u];
    data_len = (((uint32_t)p_frame[22u] << 8) | (uint32_t)p_frame[23u]);

    (void)wsprintfW(p_out, L"#%03u %s %uB", (unsigned int)(idx + 1u),
                    gb32960_cmd_desc(cmd), (unsigned int)(data_len + 25u));
    /* VIN过长挤压摘要: 仅在余量足够时附加VIN后6位 */
    if (((uint32_t)lstrlenW(p_out) + 10u) < cap) {
        wchar_t vin[8];

        memset(&vin[0], 0, sizeof(vin));
        for (i = 0u; i < 6u; i++) {
            uint8_t ch = p_frame[15u + i];   /* VIN 17位取后6位: 偏移4+11 */

            vin[i] = ((ch >= 0x20u) && (ch < 0x7Fu)) ? (wchar_t)ch : L'.';
        }
        (void)lstrcatW(p_out, L" ");
        (void)lstrcatW(p_out, &vin[0]);
    }
}
