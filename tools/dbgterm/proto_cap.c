/**
 * @file    proto_cap.c
 * @author  herongshun
 * @date    2026-11-21
 * @brief   协议行捕获实现
 *          匹配算法: 把行内全部hex字符(0-9A-Fa-f)抽出为"去空格串"并记录原行
 *          位置, 关键字同样去空格后小写化, 在去空格串中查找; 命中位置映射回
 *          原行起始点, 从该点起两两组字节(非hex字符跳过)
 */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "proto_cap.h"

/* -------------------------------------------------------------------------- */
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   判断宽字符是否为hex字符
 * @param   ch 字符
 * @return  1: 是; 0: 否
 */
static uint8_t cap_is_hex(wchar_t ch)
{
    return (((ch >= L'0') && (ch <= L'9')) ||
            ((ch >= L'A') && (ch <= L'F')) ||
            ((ch >= L'a') && (ch <= L'f'))) ? 1u : 0u;
}

/**
 * @brief   hex字符转数值
 * @param   ch hex字符
 * @return  0~15
 */
static uint8_t cap_hex_val(wchar_t ch)
{
    if ((ch >= L'0') && (ch <= L'9')) {
        return (uint8_t)(ch - L'0');
    }
    if ((ch >= L'A') && (ch <= L'F')) {
        return (uint8_t)(ch - L'A' + 10u);
    }
    return (uint8_t)(ch - L'a' + 10u);
}

/**
 * @brief   收集行内全部hex字符: 去空格串(小写) + 原行位置索引
 * @param   p_line  [in]  行文本
 * @param   p_squash [out] 去空格串(小写, 容量PROTO_CAP_BUF_MAX*2无意义, 用行内上限)
 * @param   p_pos   [out] 去空格串每个字符在原行中的索引
 * @param   max     [in]  去空格串容量
 * @return  去空格串长度
 */
static uint32_t cap_squash_hex(const wchar_t *p_line, wchar_t *p_squash,
                               uint32_t *p_pos, uint32_t max)
{
    uint32_t n = 0u;
    uint32_t i = 0u;
    uint32_t line_len = (uint32_t)lstrlenW(p_line);

    for (i = 0u; (i < line_len) && (n < max); i++) {
        wchar_t ch = p_line[i];

        if (cap_is_hex(ch) != 0u) {
            if ((ch >= L'A') && (ch <= L'F')) {
                ch = (wchar_t)(ch + 32u);   /* 小写化 */
            }
            p_squash[n] = ch;
            p_pos[n] = i;
            n++;
        }
    }
    if (n < max) {
        p_squash[n] = L'\0';
    }

    return n;
}

/**
 * @brief   关键字去空格小写化
 * @param   p_kw     [in]  关键字原文
 * @param   p_out    [out] 去空格串
 * @param   max      [in]  容量
 * @return  去空格串长度
 */
static uint32_t cap_squash_kw(const wchar_t *p_kw, wchar_t *p_out, uint32_t max)
{
    uint32_t n = 0u;
    uint32_t i = 0u;
    uint32_t kw_len = (uint32_t)lstrlenW(p_kw);

    for (i = 0u; (i < kw_len) && (n < (max - 1u)); i++) {
        wchar_t ch = p_kw[i];

        if (cap_is_hex(ch) != 0u) {
            if ((ch >= L'A') && (ch <= L'F')) {
                ch = (wchar_t)(ch + 32u);
            }
            p_out[n] = ch;
            n++;
        }
    }
    p_out[n] = L'\0';

    return n;
}

/**
 * @brief   在行内查找关键字命中(去空格匹配), 返回命中起始的原行索引
 * @param   p_line 行文本
 * @param   p_kw   关键字
 * @return  命中起始的原行索引; -1=未命中
 */
static int32_t cap_find_kw(const wchar_t *p_line, const wchar_t *p_kw)
{
    wchar_t squash[1024u];
    uint32_t pos[1024u];
    wchar_t kw_sq[PROTO_CAP_KW_WCHAR];
    uint32_t squash_len = 0u;
    uint32_t kw_len = 0u;
    uint32_t i = 0u;

    squash_len = cap_squash_hex(p_line, &squash[0], &pos[0],
                                (uint32_t)(sizeof(squash) / sizeof(squash[0])));
    kw_len = cap_squash_kw(p_kw, &kw_sq[0], PROTO_CAP_KW_WCHAR);
    if ((kw_len == 0u) || (kw_len > squash_len)) {
        return -1;
    }
    for (i = 0u; (i + kw_len) <= squash_len; i++) {
        if (memcmp(&squash[i], &kw_sq[0], (size_t)(kw_len * sizeof(wchar_t))) == 0) {
            return (int32_t)pos[i];
        }
    }

    return -1;
}

/**
 * @brief   剥除行首的 [xxx] 样式前缀(时间戳/方向标签, 最多2层)
 * @param   p_line 行文本
 * @return  剥除后的起始指针(无前缀则返回原指针)
 */
static const wchar_t *cap_skip_prefix(const wchar_t *p_line)
{
    const wchar_t *p = p_line;
    uint32_t layer = 0u;

    while ((layer < 2u) && (*p == L'[')) {
        const wchar_t *p_end = p;

        while ((*p_end != L'\0') && (*p_end != L']')) {
            p_end++;
        }
        if (*p_end != L']') {
            break;
        }
        p = p_end + 1u;
        while ((*p == L' ') || (*p == L'\t')) {
            p++;
        }
        layer++;
    }

    return p;
}

/**
 * @brief   从指定起点提取行内hex字节(两两组字节, 非hex跳过; 尾部落单忽略)
 * @param   p_start [in]     提取起点(原行指针)
 * @param   p_buf   [in/out] 字节缓冲
 * @param   p_len   [in/out] 当前字节数(进入时已有量)
 * @param   max     [in]     缓冲容量
 * @return  无
 */
static void cap_extract(const wchar_t *p_start, uint8_t *p_buf, uint32_t *p_len,
                        uint32_t max)
{
    uint8_t hi = 0u;
    uint8_t have = 0u;

    while (*p_start != L'\0') {
        if (cap_is_hex(*p_start) != 0u) {
            if (have == 0u) {
                hi = cap_hex_val(*p_start);
                have = 1u;
            } else {
                if (*p_len < max) {
                    p_buf[*p_len] = (uint8_t)((hi << 4) | cap_hex_val(*p_start));
                    (*p_len)++;
                } else {
                    return;   /* 缓冲满 */
                }
                have = 0u;
            }
        }
        p_start++;
    }
}

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

void proto_cap_init(PROTO_CAP_T *p_cap)
{
    if (p_cap == NULL) {
        return;
    }
    memset(p_cap, 0, sizeof(PROTO_CAP_T));
}

uint8_t proto_cap_feed(PROTO_CAP_T *p_cap, const PROTO_CAP_CFG_T *p_cfg,
                       const wchar_t *p_line, uint8_t *p_out, uint32_t *p_out_len)
{
    int32_t hit = -1;

    if ((p_cap == NULL) || (p_cfg == NULL) || (p_line == NULL) ||
        (p_out == NULL) || (p_out_len == NULL)) {
        return 0u;
    }
    *p_out_len = 0u;

    /* 捕获中再次命中: 先收口当前组再开新组(连续报文场景) */
    hit = cap_find_kw(p_line, &p_cfg->keyword[0]);
    if (hit >= 0) {
        uint8_t done = 0u;

        if (p_cap->active != 0u) {
            if (p_cap->len > 0u) {
                memcpy(p_out, &p_cap->buf[0], p_cap->len);
                *p_out_len = p_cap->len;
                done = 1u;
            }
            proto_cap_init(p_cap);
        }
        p_cap->active = 1u;
        p_cap->collected = 1u;
        cap_extract(&p_line[hit], &p_cap->buf[0], &p_cap->len, PROTO_CAP_BUF_MAX);
        {
            uint32_t need = ((p_cfg->lines > PROTO_CAP_LINES_MAX)
                             ? PROTO_CAP_LINES_MAX : p_cfg->lines) + 1u;

            if ((p_cap->collected >= need) || (p_cap->len >= PROTO_CAP_BUF_MAX)) {
                memcpy(p_out, &p_cap->buf[0], p_cap->len);
                *p_out_len = p_cap->len;
                proto_cap_init(p_cap);
                return 1u;
            }
        }
        return done;
    }

    if (p_cap->active == 0u) {
        return 0u;
    }

    /* 后续行: 剥前缀后全行提取 */
    cap_extract(cap_skip_prefix(p_line), &p_cap->buf[0], &p_cap->len,
                PROTO_CAP_BUF_MAX);
    p_cap->collected++;
    {
        uint32_t need = ((p_cfg->lines > PROTO_CAP_LINES_MAX)
                         ? PROTO_CAP_LINES_MAX : p_cfg->lines) + 1u;

        if ((p_cap->collected >= need) || (p_cap->len >= PROTO_CAP_BUF_MAX)) {
            memcpy(p_out, &p_cap->buf[0], p_cap->len);
            *p_out_len = p_cap->len;
            proto_cap_init(p_cap);
            return 1u;
        }
    }

    return 0u;
}
