/**
 * @file    proto_cap.c
 * @author  herongshun
 * @date    2026-11-21
 * @brief   协议行捕获实现
 *          提取算法: 行按空白切token, 仅接受"2位纯hex"token(空格分隔的
 *          hex dump, 如 "23 23 02 FE")——logcat前缀(I/yaxon ( 2035):、
 *          [2026/11/21 17:07:56.717113 INFO]:、c:0623]:等)与空行时间戳
 *          均不满足该形态, 天然不混入;
 *          匹配: 关键字同样转字节序列, 在行token字节流中按字节对齐查找,
 *          命中行从命中偏移起取字节; 空行(无有效token)不占行数窗口
 */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "proto_cap.h"

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define CAP_LINE_TOKEN_MAX  512u    /**< 单行最大token字节数 */
#define CAP_KW_BYTE_MAX     32u     /**< 关键字最大字节数 */

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
 * @brief   行token化提取字节: 按空白切token, 仅接受"2位纯hex"token
 * @param   p_line  [in]  行文本
 * @param   p_out   [out] 字节输出(容量CAP_LINE_TOKEN_MAX)
 * @return  提取字节数(0=无有效token, 视为空行)
 */
static uint32_t cap_tokenize(const wchar_t *p_line, uint8_t *p_out)
{
    uint32_t n = 0u;
    const wchar_t *p = p_line;

    while (*p != L'\0') {
        /* 跳过空白 */
        while ((*p == L' ') || (*p == L'\t')) {
            p++;
        }
        if (*p == L'\0') {
            break;
        }
        /* 收集一个token */
        {
            const wchar_t *p_start = p;
            uint32_t len = 0u;

            while ((*p != L'\0') && (*p != L' ') && (*p != L'\t')) {
                p++;
                len++;
            }
            if ((len == 2u) && (cap_is_hex(p_start[0]) != 0u) &&
                (cap_is_hex(p_start[1]) != 0u) && (n < CAP_LINE_TOKEN_MAX)) {
                p_out[n] = (uint8_t)((cap_hex_val(p_start[0]) << 4) |
                                     cap_hex_val(p_start[1]));
                n++;
            }
        }
    }

    return n;
}

/**
 * @brief   关键字转字节序列(忽略空格; 尾部落单半字节忽略)
 * @param   p_kw  [in]  关键字原文
 * @param   p_out [out] 字节序列
 * @return  字节数(0=关键字无效)
 */
static uint32_t cap_kw_bytes(const wchar_t *p_kw, uint8_t *p_out)
{
    uint32_t n = 0u;
    uint32_t i = 0u;
    uint32_t kw_len = (uint32_t)lstrlenW(p_kw);
    uint8_t hi = 0u;
    uint8_t have = 0u;

    for (i = 0u; (i < kw_len) && (n < CAP_KW_BYTE_MAX); i++) {
        if (cap_is_hex(p_kw[i]) != 0u) {
            if (have == 0u) {
                hi = cap_hex_val(p_kw[i]);
                have = 1u;
            } else {
                p_out[n] = (uint8_t)((hi << 4) | cap_hex_val(p_kw[i]));
                n++;
                have = 0u;
            }
        }
    }

    return n;
}

/**
 * @brief   在行token字节流中查找关键字字节序列(按字节对齐)
 * @param   p_line_bytes [in] 行token字节流
 * @param   line_len     [in] 字节数
 * @param   p_kw_bytes   [in] 关键字字节序列
 * @param   kw_len       [in] 关键字字节数
 * @return  命中偏移; -1=未命中
 */
static int32_t cap_find_bytes(const uint8_t *p_line_bytes, uint32_t line_len,
                              const uint8_t *p_kw_bytes, uint32_t kw_len)
{
    uint32_t i = 0u;

    if ((kw_len == 0u) || (kw_len > line_len)) {
        return -1;
    }
    for (i = 0u; (i + kw_len) <= line_len; i++) {
        if (memcmp(&p_line_bytes[i], p_kw_bytes, (size_t)kw_len) == 0) {
            return (int32_t)i;
        }
    }

    return -1;
}

/**
 * @brief   判定捕获是否应收口(窗口行数满或缓冲满)
 * @param   p_cap 捕获上下文
 * @param   p_cfg 捕获配置
 * @return  1=应收口; 0=继续
 */
static uint8_t cap_window_done(const PROTO_CAP_T *p_cap, const PROTO_CAP_CFG_T *p_cfg)
{
    uint32_t need = ((p_cfg->lines > PROTO_CAP_LINES_MAX)
                     ? PROTO_CAP_LINES_MAX : p_cfg->lines) + 1u;

    return ((p_cap->collected >= need) || (p_cap->len >= PROTO_CAP_BUF_MAX)) ? 1u : 0u;
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
    uint8_t line_bytes[CAP_LINE_TOKEN_MAX];
    uint32_t line_len = 0u;
    uint8_t kw_bytes[CAP_KW_BYTE_MAX];
    uint32_t kw_len = 0u;
    int32_t hit = -1;

    if ((p_cap == NULL) || (p_cfg == NULL) || (p_line == NULL) ||
        (p_out == NULL) || (p_out_len == NULL)) {
        return 0u;
    }
    *p_out_len = 0u;

    line_len = cap_tokenize(p_line, &line_bytes[0]);
    if (line_len == 0u) {
        return 0u;   /* 空行/无有效hex token: 不占窗口不产字节 */
    }

    kw_len = cap_kw_bytes(&p_cfg->keyword[0], &kw_bytes[0]);
    hit = cap_find_bytes(&line_bytes[0], line_len, &kw_bytes[0], kw_len);
    if (hit >= 0) {
        uint8_t done = 0u;

        /* 捕获中再次命中: 先收口当前组再开新组(连续报文不粘连) */
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
        {
            uint32_t copy = (line_len - (uint32_t)hit);

            if ((p_cap->len + copy) > PROTO_CAP_BUF_MAX) {
                copy = (PROTO_CAP_BUF_MAX - p_cap->len);
            }
            memcpy(&p_cap->buf[p_cap->len], &line_bytes[hit], copy);
            p_cap->len += copy;
        }
        if (cap_window_done(p_cap, p_cfg) != 0u) {
            memcpy(p_out, &p_cap->buf[0], p_cap->len);
            *p_out_len = p_cap->len;
            proto_cap_init(p_cap);
            return 1u;
        }
        return done;
    }

    if (p_cap->active == 0u) {
        return 0u;
    }

    /* 后续有效行: 全行token字节并入 */
    {
        uint32_t copy = line_len;

        if ((p_cap->len + copy) > PROTO_CAP_BUF_MAX) {
            copy = (PROTO_CAP_BUF_MAX - p_cap->len);
        }
        memcpy(&p_cap->buf[p_cap->len], &line_bytes[0], copy);
        p_cap->len += copy;
    }
    p_cap->collected++;
    if (cap_window_done(p_cap, p_cfg) != 0u) {
        memcpy(p_out, &p_cap->buf[0], p_cap->len);
        *p_out_len = p_cap->len;
        proto_cap_init(p_cap);
        return 1u;
    }

    return 0u;
}

uint8_t proto_cap_active(const PROTO_CAP_T *p_cap)
{
    return ((p_cap != NULL) && (p_cap->active != 0u)) ? 1u : 0u;
}

uint8_t proto_cap_take(PROTO_CAP_T *p_cap, uint8_t *p_out, uint32_t *p_out_len)
{
    if ((p_cap == NULL) || (p_out == NULL) || (p_out_len == NULL) ||
        (p_cap->active == 0u) || (p_cap->len == 0u)) {
        return 0u;
    }
    memcpy(p_out, &p_cap->buf[0], p_cap->len);
    *p_out_len = p_cap->len;
    proto_cap_init(p_cap);

    return 1u;
}
