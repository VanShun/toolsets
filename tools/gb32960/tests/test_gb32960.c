/**
 * @file    test_gb32960.c
 * @author  herongshun
 * @date    2026-11-20
 * @brief   gb32960 模块单元测试(控制台, 自构造报文验证)
 *          测试向量在测试内按协议拼帧并计算BCC, 不依赖外部样例
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include "gb32960.h"

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define TEST_OUT_MAX   (64u * 1024u)
#define TEST_PASS      0
#define TEST_FAIL      1

/* -------------------------------------------------------------------------- */
/*  模块内部全局变量                                                           */
/* -------------------------------------------------------------------------- */

static wchar_t g_out[TEST_OUT_MAX];   /**< 解析报告缓冲(宽字符) */

/* -------------------------------------------------------------------------- */
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   向帧缓冲写字节并前进
 * @param   p_buf [in,out] 帧缓冲
 * @param   p_pos [in,out] 写位置
 * @param   b     字节
 * @return  无
 */
static void put8(uint8_t *p_buf, uint32_t *p_pos, uint8_t b)
{
    p_buf[*p_pos] = b;
    (*p_pos)++;
}

/**
 * @brief   写2字节大端
 * @param   p_buf [in,out] 帧缓冲
 * @param   p_pos [in,out] 写位置
 * @param   v     WORD值
 * @return  无
 */
static void put16(uint8_t *p_buf, uint32_t *p_pos, uint16_t v)
{
    put8(p_buf, p_pos, (uint8_t)(v >> 8));
    put8(p_buf, p_pos, (uint8_t)v);
}

/**
 * @brief   写4字节大端
 * @param   p_buf [in,out] 帧缓冲
 * @param   p_pos [in,out] 写位置
 * @param   v     DWORD值
 * @return  无
 */
static void put32(uint8_t *p_buf, uint32_t *p_pos, uint32_t v)
{
    put8(p_buf, p_pos, (uint8_t)(v >> 24));
    put8(p_buf, p_pos, (uint8_t)(v >> 16));
    put8(p_buf, p_pos, (uint8_t)(v >> 8));
    put8(p_buf, p_pos, (uint8_t)v);
}

/**
 * @brief   帧收尾: 回填数据单元长度并追加BCC校验码
 * @param   p_buf [in,out] 帧缓冲
 * @param   p_pos [in,out] 写位置(数据单元末尾, 追加校验后为帧尾)
 * @param   data_len 数据单元长度
 * @return  无
 */
static void finish_frame(uint8_t *p_buf, uint32_t *p_pos, uint32_t data_len)
{
    uint32_t i = 0u;
    uint8_t bcc = 0u;

    p_buf[22] = (uint8_t)(data_len >> 8);
    p_buf[23] = (uint8_t)data_len;
    for (i = 2u; i < (24u + data_len); i++) {
        bcc ^= p_buf[i];
    }
    put8(p_buf, p_pos, bcc);
}

/**
 * @brief   构造车辆登入帧(2025版)并转hex宽文本
 * @param   p_hex [out] hex文本输出(宽字符)
 * @return  帧字节数
 */
static uint32_t build_login(wchar_t *p_hex)
{
    uint8_t buf[128u];
    uint32_t pos = 0u;
    uint32_t data_len = 0u;
    uint32_t i = 0u;
    const char *vin = "LSV1234567890AB12";
    const char *iccid = "89860112345678901234";

    put8(buf, &pos, 0x24u);
    put8(buf, &pos, 0x24u);
    put8(buf, &pos, 0x01u);        /* 命令: 车辆登入 */
    put8(buf, &pos, 0xFEu);        /* 应答: 命令包 */
    for (i = 0u; i < 17u; i++) {
        put8(buf, &pos, (uint8_t)vin[i]);
    }
    put8(buf, &pos, 0x01u);        /* 不加密 */
    put16(buf, &pos, 0u);          /* 长度占位 */

    /* 数据单元 */
    put8(buf, &pos, 26u);          /* 时间 2026-06-15 12:30:00 */
    put8(buf, &pos, 6u);
    put8(buf, &pos, 15u);
    put8(buf, &pos, 12u);
    put8(buf, &pos, 30u);
    put8(buf, &pos, 0u);
    put16(buf, &pos, 1u);          /* 流水号 */
    for (i = 0u; i < 20u; i++) {
        put8(buf, &pos, (uint8_t)iccid[i]);
    }
    put8(buf, &pos, 1u);           /* BMS数 */
    put8(buf, &pos, 1u);           /* 电池包数 */
    for (i = 0u; i < 24u; i++) {   /* 电池包编码 */
        put8(buf, &pos, (uint8_t)('A' + (i % 26u)));
    }
    data_len = pos - 24u;
    finish_frame(buf, &pos, data_len);

    for (i = 0u; i < pos; i++) {
        (void)wsprintfW(&p_hex[i * 2u], L"%02X", (unsigned int)buf[i]);
    }

    return pos;
}

/**
 * @brief   构造实时上报帧(2016版, 整车+位置信息体)
 * @param   p_hex [out] hex文本输出(宽字符)
 * @return  帧字节数
 */
static uint32_t build_report(wchar_t *p_hex)
{
    uint8_t buf[256u];
    uint32_t pos = 0u;
    uint32_t data_len = 0u;
    uint32_t i = 0u;
    const char *vin = "LSV99988877766655";

    put8(buf, &pos, 0x23u);
    put8(buf, &pos, 0x23u);        /* 2016版 */
    put8(buf, &pos, 0x02u);        /* 实时上报 */
    put8(buf, &pos, 0xFEu);
    for (i = 0u; i < 17u; i++) {
        put8(buf, &pos, (uint8_t)vin[i]);
    }
    put8(buf, &pos, 0x01u);
    put16(buf, &pos, 0u);

    /* 时间 */
    put8(buf, &pos, 26u); put8(buf, &pos, 6u); put8(buf, &pos, 15u);
    put8(buf, &pos, 13u); put8(buf, &pos, 45u); put8(buf, &pos, 30u);

    /* 整车数据 */
    put8(buf, &pos, 0x01u);        /* 信息类型 */
    put8(buf, &pos, 0x01u);        /* 车辆启动 */
    put8(buf, &pos, 0x03u);        /* 未充电 */
    put8(buf, &pos, 0x01u);        /* 纯电 */
    put16(buf, &pos, 600u);        /* 车速 60.0 */
    put32(buf, &pos, 123456u);     /* 里程 12345.6 */
    put16(buf, &pos, 3500u);       /* 总压 350.0 */
    put16(buf, &pos, 30000u);      /* 总流 0.0 */
    put8(buf, &pos, 85u);          /* SOC 85% */
    put8(buf, &pos, 0x01u);        /* DC-DC 工作 */
    put8(buf, &pos, 0x0Eu);        /* 自动D挡 */
    put16(buf, &pos, 3000u);       /* 绝缘 3000kΩ */
    put16(buf, &pos, 0xBEEFu);     /* 2016版: 绝缘后2字节预留字段 */

    /* 位置数据 */
    put8(buf, &pos, 0x05u);
    put8(buf, &pos, 0x02u);        /* 无效+南纬+西经 */
    put8(buf, &pos, 0x01u);        /* WGS84 */
    put32(buf, &pos, 121473700u);  /* 经度 121.4737 */
    put32(buf, &pos, 31247800u);   /* 纬度 31.2478 */

    data_len = pos - 24u;
    finish_frame(buf, &pos, data_len);

    for (i = 0u; i < pos; i++) {
        (void)wsprintfW(&p_hex[i * 2u], L"%02X", (unsigned int)buf[i]);
    }

    return pos;
}

/**
 * @brief   测试车辆登入帧解析
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_login(void)
{
    wchar_t hex[512u];
    uint32_t frames = 0u;

    memset(&hex[0], 0, sizeof(hex));
    (void)build_login(&hex[0]);
    frames = gb32960_parse_text(&hex[0], &g_out[0], TEST_OUT_MAX);

    if (frames != 1u) {
        printf("[FAIL] test_login: frames=%u\n", (unsigned int)frames);
        return TEST_FAIL;
    }
    if (wcsstr(&g_out[0], L"2025版") == NULL) {
        printf("[FAIL] test_login: version 2025 not detected\n");
        return TEST_FAIL;
    }
    if (wcsstr(&g_out[0], L"车辆登入") == NULL) {
        printf("[FAIL] test_login: cmd desc missing\n");
        return TEST_FAIL;
    }
    if (wcsstr(&g_out[0], L"LSV1234567890AB12") == NULL) {
        printf("[FAIL] test_login: VIN missing\n");
        return TEST_FAIL;
    }
    if (wcsstr(&g_out[0], L"ICCID: 89860112345678901234") == NULL) {
        printf("[FAIL] test_login: ICCID missing\n");
        return TEST_FAIL;
    }
    if (wcsstr(&g_out[0], L"2026-06-15 12:30:00") == NULL) {
        printf("[FAIL] test_login: time missing\n");
        return TEST_FAIL;
    }
    if (wcsstr(&g_out[0], L"校验码") == NULL) {
        printf("[FAIL] test_login: checksum line missing\n");
        return TEST_FAIL;
    }

    return TEST_PASS;
}

/**
 * @brief   测试实时上报帧(整车+位置数值换算)
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_report(void)
{
    wchar_t hex[768u];
    uint32_t frames = 0u;

    memset(&hex[0], 0, sizeof(hex));
    (void)build_report(&hex[0]);
    frames = gb32960_parse_text(&hex[0], &g_out[0], TEST_OUT_MAX);

    if (frames != 1u) {
        printf("[FAIL] test_report: frames=%u\n", (unsigned int)frames);
        return TEST_FAIL;
    }
    if (wcsstr(&g_out[0], L"2016版") == NULL) {
        printf("[FAIL] test_report: version 2016 not detected\n");
        return TEST_FAIL;
    }
    if (wcsstr(&g_out[0], L"车速: 600 (0x0258) = 60.0 km/h") == NULL) {
        printf("[FAIL] test_report: speed conversion wrong\n");
        return TEST_FAIL;
    }
    if (wcsstr(&g_out[0], L"SOC: 85%") == NULL) {
        printf("[FAIL] test_report: SOC missing\n");
        return TEST_FAIL;
    }
    if (wcsstr(&g_out[0], L"总电流: 30000 (0x7530) = 0.0 A") == NULL) {
        printf("[FAIL] test_report: current offset conversion wrong\n");
        return TEST_FAIL;
    }
    if (wcsstr(&g_out[0], L"经度: 121.473700°") == NULL) {
        printf("[FAIL] test_report: longitude conversion wrong\n");
        return TEST_FAIL;
    }
    if (wcsstr(&g_out[0], L"南纬") == NULL) {
        printf("[FAIL] test_report: gps flag wrong\n");
        return TEST_FAIL;
    }
    if (wcsstr(&g_out[0], L"绝缘电阻: 3000 (0x0BB8) = 3000 kΩ") == NULL) {
        printf("[FAIL] test_report: insulation wrong\n");
        return TEST_FAIL;
    }
    if (wcsstr(&g_out[0], L"预留字段: 0xBEEF (2字节, 2016版)") == NULL) {
        printf("[FAIL] test_report: 2016 reserved field missing\n");
        return TEST_FAIL;
    }

    return TEST_PASS;
}

/**
 * @brief   测试错误路径: 坏校验/截断/无效起始符/容错输入
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_errors(void)
{
    wchar_t hex[768u];
    uint32_t len = 0u;
    uint32_t frames = 0u;
    int32_t ret = TEST_PASS;

    /* 坏校验码 */
    memset(&hex[0], 0, sizeof(hex));
    len = build_report(&hex[0]);
    hex[(len - 1u) * 2u] = L'0';
    hex[(len - 1u) * 2u + 1u] = L'0';
    frames = gb32960_parse_text(&hex[0], &g_out[0], TEST_OUT_MAX);
    if ((frames != 0u) || (wcsstr(&g_out[0], L"校验码错误") == NULL)) {
        printf("[FAIL] test_errors: bad checksum not detected\n");
        ret = TEST_FAIL;
    }

    /* 截断 */
    memset(&hex[0], 0, sizeof(hex));
    (void)build_report(&hex[0]);
    hex[60u] = L'\0';
    frames = gb32960_parse_text(&hex[0], &g_out[0], TEST_OUT_MAX);
    if (frames != 0u) {
        printf("[FAIL] test_errors: truncated not detected\n");
        ret = TEST_FAIL;
    }

    /* 无效起始符 */
    frames = gb32960_parse_text(L"AABBCCDD", &g_out[0], TEST_OUT_MAX);
    if (frames != 0u) {
        printf("[FAIL] test_errors: bad start not rejected\n");
        ret = TEST_FAIL;
    }

    /* 容错输入: 空格+0x前缀+小写 */
    memset(&hex[0], 0, sizeof(hex));
    (void)build_report(&hex[0]);
    {
        wchar_t messy[1024u];
        uint32_t src_len = (uint32_t)lstrlenW(&hex[0]);
        uint32_t i = 0u;
        uint32_t out_pos = 0u;

        memset(&messy[0], 0, sizeof(messy));
        messy[out_pos] = L'0';
        out_pos++;
        messy[out_pos] = L'x';
        out_pos++;
        for (i = 0u; i < src_len; i++) {
            wchar_t ch = hex[i];

            if ((ch >= L'A') && (ch <= L'F')) {
                ch = (wchar_t)(ch + 32);
            }
            messy[out_pos] = ch;
            out_pos++;
            if (((i + 1u) % 16u) == 0u) {
                messy[out_pos] = L' ';
                out_pos++;
            }
            if ((out_pos + 2u) >= 1020u) {
                break;
            }
        }
        frames = gb32960_parse_text(&messy[0], &g_out[0], TEST_OUT_MAX);
        if (frames != 1u) {
            printf("[FAIL] test_errors: messy input not parsed\n");
            ret = TEST_FAIL;
        }
    }

    return ret;
}

/**
 * @brief   测试节点树接口: 切帧/单帧节点解析/帧标志/文本物化
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_nodes(void)
{
    wchar_t hex[768u];
    uint8_t bytes[GB32960_FRAME_MAX];
    uint32_t offs[4u];
    uint32_t lens[4u];
    uint32_t frames = 0u;
    uint32_t count = 0u;
    uint32_t i = 0u;
    uint32_t frame_nodes = 0u;
    uint32_t ok = 0u;
    GB32960_NODE_T nodes[512u];

    memset(&hex[0], 0, sizeof(hex));
    (void)build_report(&hex[0]);
    frames = gb32960_decode_split(&hex[0], &bytes[0], GB32960_FRAME_MAX,
                                  &offs[0], &lens[0], 4u, NULL, 0u);
    if (frames != 1u) {
        printf("[FAIL] test_nodes: split frames=%u\n", (unsigned int)frames);
        return TEST_FAIL;
    }

    memset(&nodes[0], 0, sizeof(nodes));
    ok = gb32960_parse_frame_nodes(&bytes[offs[0]], lens[0], 1u, &nodes[0],
                                   &count, 512u);
    if ((ok != 1u) || (count == 0u)) {
        printf("[FAIL] test_nodes: frame parse ok=%u count=%u\n",
               (unsigned int)ok, (unsigned int)count);
        return TEST_FAIL;
    }
    if ((nodes[0].flags & GB32960_NODE_FLAG_FRAME) == 0u) {
        printf("[FAIL] test_nodes: frame flag missing on title node\n");
        return TEST_FAIL;
    }
    if (nodes[0].depth != 0u) {
        printf("[FAIL] test_nodes: title depth=%u\n", (unsigned int)nodes[0].depth);
        return TEST_FAIL;
    }
    /* 逐节点校验: 深度不得超过前驱+1(树连续性) */
    for (i = 1u; i < count; i++) {
        if (nodes[i].depth > (uint8_t)(nodes[i - 1u].depth + 1u)) {
            printf("[FAIL] test_nodes: depth jump at %u\n", (unsigned int)i);
            return TEST_FAIL;
        }
        if ((nodes[i].flags & GB32960_NODE_FLAG_FRAME) != 0u) {
            printf("[FAIL] test_nodes: unexpected frame flag at %u\n", (unsigned int)i);
            return TEST_FAIL;
        }
    }
    frame_nodes = count;

    /* 追加第二帧(同帧重复解析): 帧标志应出现在追加段首 */
    ok = gb32960_parse_frame_nodes(&bytes[offs[0]], lens[0], 2u, &nodes[0],
                                   &count, 512u);
    if ((ok != 1u) || (count <= frame_nodes)) {
        printf("[FAIL] test_nodes: append ok=%u count=%u\n",
               (unsigned int)ok, (unsigned int)count);
        return TEST_FAIL;
    }
    if ((nodes[frame_nodes].flags & GB32960_NODE_FLAG_FRAME) == 0u) {
        printf("[FAIL] test_nodes: appended frame flag missing\n");
        return TEST_FAIL;
    }

    /* 文本物化: 内容等价(可检索到字段行) */
    memset(&g_out[0], 0, sizeof(g_out));
    gb32960_nodes_to_text(&nodes[0], count, &g_out[0], TEST_OUT_MAX);
    if (wcsstr(&g_out[0], L"预留字段: 0xBEEF (2字节, 2016版)") == NULL) {
        printf("[FAIL] test_nodes: text materialize missing reserved\n");
        return TEST_FAIL;
    }
    if (wcsstr(&g_out[0], L"第 2 帧") == NULL) {
        printf("[FAIL] test_nodes: appended frame text missing\n");
        return TEST_FAIL;
    }

    return TEST_PASS;
}

/**
 * @brief   测试入口
 * @param   无
 * @return  0: 全部通过; 1: 存在失败
 */
int main(void)
{
    int32_t failed = 0;

    failed += test_login();
    failed += test_report();
    failed += test_errors();
    failed += test_nodes();

    if (failed == 0) {
        printf("[PASS] all gb32960 tests passed\n");
        return 0;
    }
    printf("[FAIL] %d gb32960 test(s) failed\n", failed);

    return 1;
}
