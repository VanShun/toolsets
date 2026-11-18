/**
 * @file    test_hex_conv.c
 * @author  herongshun
 * @date    2026-10-26
 * @brief   hex_conv 模块单元测试(控制台)
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "hex_conv.h"

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define TEST_BUF_BYTES      16u     /**< 测试缓冲区容量 */
#define TEST_BUF_CHARS      64u     /**< 测试字符串缓冲容量 */
#define TEST_PASS           0       /**< 单项测试通过 */
#define TEST_FAIL           1       /**< 单项测试失败 */

/* -------------------------------------------------------------------------- */
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   测试Hex字符串基本解析(分隔符/大小写/紧凑格式)
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_parse_basic(void)
{
    uint8_t  bytes[TEST_BUF_BYTES] = {0};
    uint32_t count = 0u;
    uint32_t err_pos = 0u;
    HEX_CONV_RET_E ret = HEX_CONV_RET_OK;

    /* 空格分隔 */
    memset(bytes, 0, sizeof(bytes));
    ret = hex_conv_hex_str_to_bytes("48 65 6C 6C 6F", bytes, TEST_BUF_BYTES, &count, &err_pos);
    if ((ret != HEX_CONV_RET_OK) || (count != 5u) || (memcmp(bytes, "Hello", 5u) != 0)) {
        printf("[FAIL] test_parse_basic: space separated\n");
        return TEST_FAIL;
    }

    /* 紧凑格式 + 大小写混合 */
    memset(bytes, 0, sizeof(bytes));
    ret = hex_conv_hex_str_to_bytes("0A0aAb", bytes, TEST_BUF_BYTES, &count, &err_pos);
    if ((ret != HEX_CONV_RET_OK) || (count != 3u) ||
        (bytes[0] != 0x0Au) || (bytes[1] != 0x0Au) || (bytes[2] != 0xABu)) {
        printf("[FAIL] test_parse_basic: compact & mixed case\n");
        return TEST_FAIL;
    }

    /* 多种分隔符混合 */
    memset(bytes, 0, sizeof(bytes));
    ret = hex_conv_hex_str_to_bytes("48,65;6C\n6C\t6F", bytes, TEST_BUF_BYTES, &count, &err_pos);
    if ((ret != HEX_CONV_RET_OK) || (count != 5u) || (memcmp(bytes, "Hello", 5u) != 0)) {
        printf("[FAIL] test_parse_basic: mixed separators\n");
        return TEST_FAIL;
    }

    /* 空串解析为0字节 */
    memset(bytes, 0, sizeof(bytes));
    ret = hex_conv_hex_str_to_bytes("", bytes, TEST_BUF_BYTES, &count, &err_pos);
    if ((ret != HEX_CONV_RET_OK) || (count != 0u)) {
        printf("[FAIL] test_parse_basic: empty string\n");
        return TEST_FAIL;
    }

    printf("[PASS] test_parse_basic\n");
    return TEST_PASS;
}

/**
 * @brief   测试Hex字符串解析错误分支(非法字符/奇数位数/溢出/入参)
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_parse_errors(void)
{
    uint8_t  bytes[TEST_BUF_BYTES] = {0};
    uint32_t count = 0u;
    uint32_t err_pos = 0u;
    HEX_CONV_RET_E ret = HEX_CONV_RET_OK;

    /* 非法字符: 第2个字符'G' */
    ret = hex_conv_hex_str_to_bytes("4G", bytes, TEST_BUF_BYTES, &count, &err_pos);
    if ((ret != HEX_CONV_RET_ERR_FORMAT) || (err_pos != 1u)) {
        printf("[FAIL] test_parse_errors: invalid char\n");
        return TEST_FAIL;
    }

    /* 奇数位数 */
    ret = hex_conv_hex_str_to_bytes("123", bytes, TEST_BUF_BYTES, &count, &err_pos);
    if (ret != HEX_CONV_RET_ERR_FORMAT) {
        printf("[FAIL] test_parse_errors: odd digits\n");
        return TEST_FAIL;
    }

    /* 缓冲区溢出: 容量2字节装不下3字节 */
    ret = hex_conv_hex_str_to_bytes("010203", bytes, 2u, &count, &err_pos);
    if (ret != HEX_CONV_RET_ERR_OVERFLOW) {
        printf("[FAIL] test_parse_errors: overflow\n");
        return TEST_FAIL;
    }

    /* 入参非法 */
    ret = hex_conv_hex_str_to_bytes(NULL, bytes, TEST_BUF_BYTES, &count, &err_pos);
    if (ret != HEX_CONV_RET_ERR_PARAM) {
        printf("[FAIL] test_parse_errors: null param\n");
        return TEST_FAIL;
    }

    printf("[PASS] test_parse_errors\n");
    return TEST_PASS;
}

/**
 * @brief   测试字节序列格式化为十六进制字符串
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_format(void)
{
    uint8_t  bytes[TEST_BUF_BYTES] = {0x48u, 0x65u, 0x0Au, 0xABu};
    char     out[TEST_BUF_CHARS] = {0};
    HEX_CONV_RET_E ret = HEX_CONV_RET_OK;

    ret = hex_conv_bytes_to_hex_str(bytes, 4u, out, TEST_BUF_CHARS);
    if ((ret != HEX_CONV_RET_OK) || (strcmp(out, "48 65 0A AB") != 0)) {
        printf("[FAIL] test_format: normal, got \"%s\"\n", out);
        return TEST_FAIL;
    }

    /* 单字节无尾随空格 */
    memset(out, 0, sizeof(out));
    ret = hex_conv_bytes_to_hex_str(bytes, 1u, out, TEST_BUF_CHARS);
    if ((ret != HEX_CONV_RET_OK) || (strcmp(out, "48") != 0)) {
        printf("[FAIL] test_format: single byte\n");
        return TEST_FAIL;
    }

    /* 容量不足 */
    ret = hex_conv_bytes_to_hex_str(bytes, 4u, out, 5u);
    if (ret != HEX_CONV_RET_ERR_OVERFLOW) {
        printf("[FAIL] test_format: overflow\n");
        return TEST_FAIL;
    }

    /* 0字节 */
    ret = hex_conv_bytes_to_hex_str(bytes, 0u, out, TEST_BUF_CHARS);
    if (ret != HEX_CONV_RET_ERR_OVERFLOW) {
        printf("[FAIL] test_format: zero count\n");
        return TEST_FAIL;
    }

    printf("[PASS] test_format\n");
    return TEST_PASS;
}

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   测试入口: 执行全部用例并汇总结果
 * @param   无
 * @return  0: 全部通过; 1: 存在失败
 */
int main(void)
{
    int32_t fails = 0;

    fails += test_parse_basic();
    fails += test_parse_errors();
    fails += test_format();

    if (fails == 0) {
        printf("[PASS] all tests passed\n");
        return 0;
    }

    printf("[FAIL] %d test case(s) failed\n", fails);
    return 1;
}
