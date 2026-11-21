/**
 * @file    proto_link.c
 * @author  herongshun
 * @date    2026-11-21
 * @brief   协议解析装配层实现(仅合集版编译)
 *          桥接: dbgterm协议捕获窗口(字节流) <-> gb32960解析核心(hex文本)
 */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "gb32960.h"
#include "proto_view.h"
#include "proto_link.h"

/* -------------------------------------------------------------------------- */
/*  模块内部全局变量                                                           */
/* -------------------------------------------------------------------------- */

static wchar_t g_pl_hex[(GB32960_FRAME_MAX * 2u) + 4u];  /**< 字节转hex文本缓冲 */

/* -------------------------------------------------------------------------- */
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   gb32960解析适配: 字节流 -> hex文本 -> 解析报告
 * @param   p_data  [in]  报文字节
 * @param   len     [in]  字节数(超上限截断, 解析器会在报告中标注)
 * @param   p_out   [out] 报告输出
 * @param   out_cap [in]  输出容量(宽字符)
 * @return  成功解析的帧数
 */
static uint32_t pl_gb32960_parse(const uint8_t *p_data, uint32_t len,
                                 wchar_t *p_out, uint32_t out_cap)
{
    uint32_t i = 0u;

    if ((p_data == NULL) || (len == 0u) || (p_out == NULL) || (out_cap < 64u)) {
        return 0u;
    }
    if (len > GB32960_FRAME_MAX) {
        len = GB32960_FRAME_MAX;   /* 解析器输入上限, 截断 */
    }
    for (i = 0u; i < len; i++) {
        (void)wsprintfW(&g_pl_hex[i * 2u], L"%02X", (unsigned int)p_data[i]);
    }
    g_pl_hex[len * 2u] = L'\0';

    return gb32960_parse_text(&g_pl_hex[0], p_out, out_cap);
}

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

void proto_link_init(void)
{
    proto_view_set_parser(L"GB/T 32960", pl_gb32960_parse);
}
