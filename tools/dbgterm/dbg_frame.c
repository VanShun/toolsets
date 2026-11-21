/**
 * @file    dbg_frame.c
 * @author  herongshun
 * @date    2026-11-06
 * @brief   接收流处理实现: 行状态机 + 解码 + 时间戳前缀
 */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "dbg_frame.h"

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define DBG_FRAME_TRUE    1u   /**< 布尔真 */
#define DBG_FRAME_FALSE   0u   /**< 布尔假 */
#define DBG_FRAME_CRLF_W  2u   /**< CRLF宽字符数 */

/* -------------------------------------------------------------------------- */
/*  内部类型                                                                    */
/* -------------------------------------------------------------------------- */

/** 输出游标上下文 */
typedef struct DBG_FRAME_OUT_TAG
{
    wchar_t *p_buf;     /**< 输出缓冲 */
    uint32_t cap;       /**< 容量(宽字符) */
    uint32_t pos;       /**< 当前写入位置 */
} DBG_FRAME_OUT_T;

/* -------------------------------------------------------------------------- */
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   追加宽字符到输出(带容量与安全余量保护)
 * @param   p_ctx  输出上下文
 * @param   p_text 待追加文本
 * @return  无
 */
static void frame_out_append(DBG_FRAME_OUT_T *p_ctx, const wchar_t *p_text)
{
    uint32_t len = (uint32_t)lstrlenW(p_text);

    if ((p_ctx->pos + len + DBG_FRAME_OUT_MARGIN) > p_ctx->cap) {
        return;
    }
    memcpy(&p_ctx->p_buf[p_ctx->pos], p_text, len * sizeof(wchar_t));
    p_ctx->pos += len;
}

/**
 * @brief   判断字节是否为ASCII数字
 * @param   ch 字节
 * @return  1: 是; 0: 否
 */
static uint8_t frame_is_digit(uint8_t ch)
{
    return ((ch >= (uint8_t)'0') && (ch <= (uint8_t)'9')) ? 1u : 0u;
}

/**
 * @brief   剥除行首设备侧时间戳(logcat -v time格式"MM-DD HH:MM:SS.mmm ", 19字节)
 * @param   p_line 行字节缓冲
 * @param   p_len  [in/out] 行长度(命中时前移19)
 * @return  剥除后的数据起始指针
 */
static const uint8_t *frame_strip_logcat_ts(const uint8_t *p_line, uint32_t *p_len)
{
    /* 形如 "11-17 15:18:21.149 " 的固定模板逐位校验(18字符模板+空格=19) */
    static const uint8_t expect[18u] =
    {
        (uint8_t)'D', (uint8_t)'D', (uint8_t)'-', (uint8_t)'D', (uint8_t)'D', (uint8_t)' ',
        (uint8_t)'D', (uint8_t)'D', (uint8_t)':', (uint8_t)'D', (uint8_t)'D', (uint8_t)':',
        (uint8_t)'D', (uint8_t)'D', (uint8_t)'.', (uint8_t)'D', (uint8_t)'D', (uint8_t)'D'
    };
    uint32_t i = 0u;

    if (*p_len < 20u) {   /* 19字节模板 + 至少1字节内容 */
        return p_line;
    }
    for (i = 0u; i < 18u; i++) {
        if (expect[i] == (uint8_t)'D') {
            if (frame_is_digit(p_line[i]) == 0u) {
                return p_line;
            }
        } else if (p_line[i] != expect[i]) {
            return p_line;
        }
    }
    if (p_line[18u] != (uint8_t)' ') {
        return p_line;   /* 模板后应为空格分隔 */
    }

    *p_len -= 19u;
    return &p_line[19u];
}

/**
 * @brief   输出一行: 可选时间戳前缀(方向标签可空) + 解码行文本 + CRLF, 并复位半行缓冲
 * @param   p_frame 装配上下文
 * @param   p_opt   装配选项
 * @param   p_ctx   输出上下文
 * @return  无
 */
static void frame_emit_line(DBG_FRAME_T *p_frame, const DBG_FRAME_OPT_T *p_opt,
                            DBG_FRAME_OUT_T *p_ctx)
{
    SYSTEMTIME st;
    int32_t conv_len = 0;
    uint32_t free_wide = 0u;
    const uint8_t *p_src = &p_frame->line[0];
    uint32_t src_len = p_frame->line_len;

    memset(&st, 0, sizeof(st));
    if ((p_ctx->pos + p_frame->line_len + DBG_FRAME_OUT_MARGIN) > p_ctx->cap) {
        return;   /* 输出缓冲不足: 本轮不输出, 数据保留在半行缓冲 */
    }

    if (p_opt->strip_dev_ts != DBG_FRAME_FALSE) {
        p_src = frame_strip_logcat_ts(&p_frame->line[0], &src_len);
    }

    if (p_opt->ts_enable != DBG_FRAME_FALSE) {
        GetLocalTime(&st);
        if (p_opt->p_dir_tag != NULL) {
            (void)wsprintfW(&p_ctx->p_buf[p_ctx->pos], L"[%02u:%02u:%02u.%03u][%s] ",
                            (unsigned int)st.wHour, (unsigned int)st.wMinute,
                            (unsigned int)st.wSecond, (unsigned int)st.wMilliseconds,
                            p_opt->p_dir_tag);
        } else {
            (void)wsprintfW(&p_ctx->p_buf[p_ctx->pos], L"[%02u:%02u:%02u.%03u] ",
                            (unsigned int)st.wHour, (unsigned int)st.wMinute,
                            (unsigned int)st.wSecond, (unsigned int)st.wMilliseconds);
        }
        p_ctx->pos += (uint32_t)lstrlenW(&p_ctx->p_buf[p_ctx->pos]);
    }

    free_wide = (p_ctx->cap - DBG_FRAME_OUT_MARGIN) - p_ctx->pos;
    if (src_len > 0u) {
        conv_len = MultiByteToWideChar((p_opt->enc == DBG_FRAME_ENC_UTF8) ? CP_UTF8 : CP_ACP,
                                       0u, (LPCCH)p_src, (int32_t)src_len,
                                       &p_ctx->p_buf[p_ctx->pos], (int32_t)free_wide);
        if (conv_len > 0) {
            p_ctx->pos += (uint32_t)conv_len;
        } else {
            frame_out_append(p_ctx, L"(解码失败)");
        }
    }
    frame_out_append(p_ctx, L"\r\n");

    p_frame->line_len = 0u;
}

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

void dbg_frame_init(DBG_FRAME_T *p_frame)
{
    if (p_frame == NULL) {
        return;
    }
    memset(p_frame, 0, sizeof(DBG_FRAME_T));
    p_frame->last_byte_tick = GetTickCount();
}

uint32_t dbg_frame_feed(DBG_FRAME_T *p_frame, const uint8_t *p_data, uint32_t len,
                        const DBG_FRAME_OPT_T *p_opt,
                        wchar_t *p_out, uint32_t out_max, uint32_t *p_out_pos)
{
    DBG_FRAME_OUT_T out_ctx;
    uint32_t consumed = 0u;
    uint32_t index = 0u;
    uint8_t ch = 0u;

    if ((p_frame == NULL) || (p_data == NULL) || (p_opt == NULL) ||
        (p_out == NULL) || (p_out_pos == NULL) ||
        (out_max <= DBG_FRAME_OUT_MARGIN) || (*p_out_pos >= out_max)) {
        return 0u;
    }

    out_ctx.p_buf = p_out;
    out_ctx.cap = out_max;
    out_ctx.pos = *p_out_pos;
    p_frame->last_byte_tick = GetTickCount();

    for (index = 0u; index < len; index++) {
        ch = p_data[index];

        if (ch == (uint8_t)'\r') {
            frame_emit_line(p_frame, p_opt, &out_ctx);
            p_frame->cr_pending = DBG_FRAME_TRUE;
        } else if (ch == (uint8_t)'\n') {
            if (p_frame->cr_pending == DBG_FRAME_FALSE) {
                frame_emit_line(p_frame, p_opt, &out_ctx);
            }
            p_frame->cr_pending = DBG_FRAME_FALSE;
        } else {
            if (p_frame->cr_pending == DBG_FRAME_TRUE) {
                p_frame->cr_pending = DBG_FRAME_FALSE;   /* CR后跟普通字符: 新行已开始 */
            }
            if (p_frame->line_len >= DBG_FRAME_LINE_MAX) {
                frame_emit_line(p_frame, p_opt, &out_ctx);   /* 超长强制断行 */
            }
            if ((out_ctx.pos + DBG_FRAME_LINE_MAX + DBG_FRAME_OUT_MARGIN) > out_ctx.cap) {
                break;   /* 输出缓冲将满, 停止消费, 余量下轮再喂 */
            }
            p_frame->line[p_frame->line_len] = ch;
            p_frame->line_len++;
        }
        consumed = index + 1u;
    }

    *p_out_pos = out_ctx.pos;
    if (out_ctx.pos < out_ctx.cap) {
        p_out[out_ctx.pos] = L'\0';   /* 终结输出文本, 便于调用方按C字符串使用 */
    }

    return consumed;
}

uint32_t dbg_frame_flush_idle(DBG_FRAME_T *p_frame, const DBG_FRAME_OPT_T *p_opt,
                              wchar_t *p_out, uint32_t out_max, uint32_t *p_out_pos)
{
    DBG_FRAME_OUT_T out_ctx;
    uint32_t now = 0u;

    if ((p_frame == NULL) || (p_opt == NULL) || (p_out == NULL) || (p_out_pos == NULL) ||
        (out_max <= DBG_FRAME_OUT_MARGIN) || (*p_out_pos >= out_max)) {
        return DBG_FRAME_FALSE;
    }
    if (p_frame->line_len == 0u) {
        return DBG_FRAME_FALSE;
    }

    now = GetTickCount();
    if ((now - p_frame->last_byte_tick) < DBG_FRAME_IDLE_MS) {
        return DBG_FRAME_FALSE;
    }

    out_ctx.p_buf = p_out;
    out_ctx.cap = out_max;
    out_ctx.pos = *p_out_pos;
    frame_emit_line(p_frame, p_opt, &out_ctx);

    *p_out_pos = out_ctx.pos;
    if (out_ctx.pos < out_ctx.cap) {
        p_out[out_ctx.pos] = L'\0';
    }

    return DBG_FRAME_TRUE;
}

uint8_t dbg_frame_idle_ready(const DBG_FRAME_T *p_frame)
{
    uint32_t now = 0u;

    if ((p_frame == NULL) || (p_frame->line_len == 0u)) {
        return 0u;
    }

    now = GetTickCount();
    return ((now - p_frame->last_byte_tick) >= DBG_FRAME_IDLE_MS) ? 1u : 0u;
}
