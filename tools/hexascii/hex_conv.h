/**
 * @file    hex_conv.h
 * @author  herongshun
 * @date    2026-10-26
 * @brief   十六进制与字节序列转换核心逻辑接口(纯C, 无界面依赖)
 */

#ifndef HEX_CONV_H
#define HEX_CONV_H

#include <stdint.h>

/** 转换结果码 */
typedef enum HEX_CONV_RET_TAG
{
    HEX_CONV_RET_OK,            /**< 转换成功 */
    HEX_CONV_RET_ERR_PARAM,     /**< 入参非法 */
    HEX_CONV_RET_ERR_FORMAT,    /**< 十六进制字符串格式非法 */
    HEX_CONV_RET_ERR_OVERFLOW,  /**< 输出缓冲区空间不足 */
} HEX_CONV_RET_E;

/** 单次转换最大字节数 */
#define HEX_CONV_MAX_BYTES      8192u

/** 十六进制输出字符串最大长度(每字节2字符+1空格, 含结束符) */
#define HEX_CONV_HEX_STR_MAX    ((HEX_CONV_MAX_BYTES * 3u) + 1u)

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
                                         uint32_t *p_err_pos);

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
                                         uint32_t out_max);

#endif /* HEX_CONV_H */
