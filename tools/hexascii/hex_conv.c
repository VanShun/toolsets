/**
 * @file    hex_conv.c
 * @author  herongshun
 * @date    2026-10-26
 * @brief   十六进制与字节序列转换核心逻辑实现
 */

#include <stdint.h>
#include <stddef.h>
#include "hex_conv.h"

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define HEX_CONV_TABLE_LEN    16u    /**< 十六进制查找表长度 */
#define HEX_CONV_NIBBLE_SHIFT 4u     /**< 高半字节移位数 */
#define HEX_CONV_NIBBLE_MASK  0x0Fu  /**< 半字节掩码 */
#define HEX_CONV_TRUE         1u     /**< 布尔真 */
#define HEX_CONV_FALSE        0u     /**< 布尔假 */

/* -------------------------------------------------------------------------- */
/*  内部查找表                                                                 */
/* -------------------------------------------------------------------------- */

static const char g_hex_conv_digit_table[HEX_CONV_TABLE_LEN] = "0123456789ABCDEF"; /**< 十六进制字符查找表 */

/* -------------------------------------------------------------------------- */
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   判断字符是否为十六进制串中允许的分隔符(空格/制表/回车/换行/逗号/分号)
 * @param   ch  待判断字符
 * @return  1: 是分隔符; 0: 非分隔符
 */
static uint8_t hex_conv_is_separator(uint8_t ch)
{
    uint8_t is_sep = HEX_CONV_FALSE;

    switch (ch)
    {
        case ' ':       /* fall through */
        case '\t':      /* fall through */
        case '\r':      /* fall through */
        case '\n':      /* fall through */
        case ',':       /* fall through */
        case ';':
        {
            is_sep = HEX_CONV_TRUE;
            break;
        }
        default:
        {
            is_sep = HEX_CONV_FALSE;
            break;
        }
    }

    return is_sep;
}

/**
 * @brief   将单个十六进制字符转换为数值
 * @param   ch      待转换字符
 * @param   p_val   [out] 转换结果(0~15)
 * @return  1: 转换成功; 0: 非十六进制字符
 */
static uint8_t hex_conv_digit_to_val(uint8_t ch, uint8_t *p_val)
{
    uint8_t ok = HEX_CONV_FALSE;

    if ((ch >= '0') && (ch <= '9')) {
        *p_val = (uint8_t)(ch - '0');
        ok = HEX_CONV_TRUE;
    } else if ((ch >= 'A') && (ch <= 'F')) {
        *p_val = (uint8_t)(ch - 'A' + 10u);
        ok = HEX_CONV_TRUE;
    } else if ((ch >= 'a') && (ch <= 'f')) {
        *p_val = (uint8_t)(ch - 'a' + 10u);
        ok = HEX_CONV_TRUE;
    } else {
        ok = HEX_CONV_FALSE;
    }

    return ok;
}

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   将十六进制字符串解析为字节序列, 自动跳过空格/逗号/分号等分隔符
 * @param   p_hex_str     [in]  十六进制字符串(以'\0'结尾)
 * @param   p_bytes       [out] 字节序列输出缓冲区
 * @param   max_bytes     [in]  输出缓冲区容量(字节)
 * @param   p_byte_count  [out] 实际解析出的字节数
 * @param   p_err_pos     [out] 出错字符位置索引(可为NULL)
 * @return  HEX_CONV_RET_E 转换结果码
 */
HEX_CONV_RET_E hex_conv_hex_str_to_bytes(const char *p_hex_str,
                                         uint8_t *p_bytes,
                                         uint32_t max_bytes,
                                         uint32_t *p_byte_count,
                                         uint32_t *p_err_pos)
{
    uint32_t scan_index = 0u;
    uint32_t nibble_hi  = 0u;
    uint8_t  has_hi     = HEX_CONV_FALSE;
    uint8_t  digit_val  = 0u;

    if ((p_hex_str == NULL) || (p_bytes == NULL) || (p_byte_count == NULL)) {
        return HEX_CONV_RET_ERR_PARAM;
    }

    *p_byte_count = 0u;
    if (p_err_pos != NULL) {
        *p_err_pos = 0u;
    }

    for (scan_index = 0u; p_hex_str[scan_index] != '\0'; scan_index++) {
        uint8_t ch = (uint8_t)p_hex_str[scan_index];

        if (hex_conv_is_separator(ch) == HEX_CONV_TRUE) {
            continue;
        }

        if (hex_conv_digit_to_val(ch, &digit_val) == HEX_CONV_FALSE) {
            if (p_err_pos != NULL) {
                *p_err_pos = scan_index;
            }
            return HEX_CONV_RET_ERR_FORMAT;
        }

        if (has_hi == HEX_CONV_FALSE) {
            nibble_hi = digit_val;
            has_hi = HEX_CONV_TRUE;
        } else {
            if (*p_byte_count >= max_bytes) {
                if (p_err_pos != NULL) {
                    *p_err_pos = scan_index;
                }
                return HEX_CONV_RET_ERR_OVERFLOW;
            }
            p_bytes[*p_byte_count] = (uint8_t)((nibble_hi << HEX_CONV_NIBBLE_SHIFT) | digit_val);
            (*p_byte_count)++;
            has_hi = HEX_CONV_FALSE;
        }
    }

    if (has_hi == HEX_CONV_TRUE) {
        /* 半字节悬空: 十六进制位数为奇数 */
        if (p_err_pos != NULL) {
            *p_err_pos = scan_index;
        }
        return HEX_CONV_RET_ERR_FORMAT;
    }

    return HEX_CONV_RET_OK;
}

/**
 * @brief   将字节序列格式化为大写十六进制字符串, 字节间以空格分隔
 * @param   p_bytes     [in]  字节序列
 * @param   byte_count  [in]  字节数
 * @param   p_out       [out] 十六进制字符串输出缓冲区
 * @param   out_max     [in]  输出缓冲区容量(字符)
 * @return  HEX_CONV_RET_E 转换结果码
 */
HEX_CONV_RET_E hex_conv_bytes_to_hex_str(const uint8_t *p_bytes,
                                         uint32_t byte_count,
                                         char *p_out,
                                         uint32_t out_max)
{
    uint32_t i   = 0u;
    uint32_t idx = 0u;

    if ((p_bytes == NULL) || (p_out == NULL)) {
        return HEX_CONV_RET_ERR_PARAM;
    }

    if ((byte_count == 0u) || ((byte_count * 3u) > out_max)) {
        return HEX_CONV_RET_ERR_OVERFLOW;
    }

    for (i = 0u; i < byte_count; i++) {
        p_out[idx] = g_hex_conv_digit_table[(p_bytes[i] >> HEX_CONV_NIBBLE_SHIFT) & HEX_CONV_NIBBLE_MASK];
        idx++;
        p_out[idx] = g_hex_conv_digit_table[p_bytes[i] & HEX_CONV_NIBBLE_MASK];
        idx++;

        if ((i + 1u) < byte_count) {
            p_out[idx] = ' ';
            idx++;
        }
    }
    p_out[idx] = '\0';

    return HEX_CONV_RET_OK;
}
