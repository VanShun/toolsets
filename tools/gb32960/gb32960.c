/**
 * @file    gb32960.c
 * @author  herongshun
 * @date    2026-11-20
 * @brief   GB/T 32960.3-2025 协议解析核心实现
 *          帧结构: 起始符(2) 命令(1) 应答(1) VIN(17) 加密(1) 长度(2) 数据单元(N) 校验(1)
 *          校验: BCC异或(从命令标识到数据单元末尾)
 *          解析直接产出缩进文本报告; 有符号64位定点换算(支持负值偏移);
 *          全程边界检查, 截断时优雅标注
 */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "gb32960.h"

/* -------------------------------------------------------------------------- */
/*  宏定义: 帧结构与解析参数                                                   */
/* -------------------------------------------------------------------------- */
#define GB_HDR_LEN          24u     /**< 帧头长度(不含校验码) */
#define GB_VIN_LEN          17u     /**< VIN长度 */
#define GB_LINE_MAX         512u    /**< 单行格式化缓冲宽字符容量 */
#define GB_LIST_MAX         64u     /**< 列表条目上限(电机/电池包/探针等) */

/* -------------------------------------------------------------------------- */
/*  内部类型                                                                    */
/* -------------------------------------------------------------------------- */

/** 输出上下文 */
typedef struct GB_OUT_TAG
{
    wchar_t *p_buf;     /**< 输出缓冲 */
    uint32_t cap;       /**< 容量 */
    uint32_t pos;       /**< 写位置 */
    uint32_t frames;    /**< 成功帧计数 */
} GB_OUT_T;

/** 解析游标(带边界保护) */
typedef struct GB_CUR_TAG
{
    const uint8_t *p_data;  /**< 数据指针 */
    uint32_t len;           /**< 总长 */
    uint32_t off;           /**< 当前偏移 */
    uint8_t truncated;      /**< 截断标志 */
} GB_CUR_T;

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 输出辅助                                                     */
/* -------------------------------------------------------------------------- */

/**
 * @brief   追加宽字符串到输出
 * @param   p_ctx  输出上下文
 * @param   p_text 文本
 * @return  无
 */
static void out_str(GB_OUT_T *p_ctx, const wchar_t *p_text)
{
    uint32_t len = (uint32_t)lstrlenW(p_text);

    if ((p_ctx->pos + len + 1u) >= p_ctx->cap) {
        return;
    }
    memcpy(&p_ctx->p_buf[p_ctx->pos], p_text, len * sizeof(wchar_t));
    p_ctx->pos += len;
    p_ctx->p_buf[p_ctx->pos] = L'\0';
}

/**
 * @brief   追加缩进 + 文本行(带CRLF)
 * @param   p_ctx  输出上下文
 * @param   indent 缩进层级(0=根)
 * @param   p_text 行文本(已格式化)
 * @return  无
 */
static void out_line(GB_OUT_T *p_ctx, uint32_t indent, const wchar_t *p_text)
{
    uint32_t i = 0u;

    for (i = 0u; i < indent; i++) {
        out_str(p_ctx, L"  ");
    }
    out_str(p_ctx, p_text);
    out_str(p_ctx, L"\r\n");
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 读取辅助(边界保护)                                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief   读1字节(越界置截断标志并返回0)
 * @param   p_cur 游标
 * @return  字节值
 */
static uint8_t rd_u8(GB_CUR_T *p_cur)
{
    if (p_cur->off >= p_cur->len) {
        p_cur->truncated = 1u;
        return 0u;
    }
    p_cur->off++;
    return p_cur->p_data[p_cur->off - 1u];
}

/**
 * @brief   读2字节大端(越界保护)
 * @param   p_cur 游标
 * @return  WORD值
 */
static uint16_t rd_u16(GB_CUR_T *p_cur)
{
    uint16_t hi = (uint16_t)rd_u8(p_cur);
    uint16_t lo = (uint16_t)rd_u8(p_cur);

    return (uint16_t)((hi << 8) | lo);
}

/**
 * @brief   读4字节大端(越界保护)
 * @param   p_cur 游标
 * @return  DWORD值
 */
static uint32_t rd_u32(GB_CUR_T *p_cur)
{
    uint32_t b0 = (uint32_t)rd_u8(p_cur);
    uint32_t b1 = (uint32_t)rd_u8(p_cur);
    uint32_t b2 = (uint32_t)rd_u8(p_cur);
    uint32_t b3 = (uint32_t)rd_u8(p_cur);

    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}

/**
 * @brief   读N字节转宽字符串(ASCII, 不可打印转'.')
 * @param   p_cur 游标
 * @param   p_out 输出缓冲
 * @param   max   容量
 * @param   n     读取字节数
 * @return  无
 */
static void rd_str(GB_CUR_T *p_cur, wchar_t *p_out, uint32_t max, uint32_t n)
{
    uint32_t i = 0u;

    memset(p_out, 0, max * sizeof(wchar_t));
    for (i = 0u; i < n; i++) {
        uint8_t ch = rd_u8(p_cur);

        if (i < (max - 1u)) {
            p_out[i] = ((ch >= 0x20u) && (ch < 0x7Fu)) ? (wchar_t)ch : L'.';
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 数值格式化(64位有符号定点, 支持负偏移)                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief   64位有符号转十进制宽字符串(wsprintfW不支持64位)
 * @param   p_out [out] 输出缓冲(至少24宽字符)
 * @param   v     数值
 * @return  无
 */
static void w_i64(wchar_t *p_out, int64_t v)
{
    wchar_t tmp[24u];
    uint32_t i = 0u;
    uint64_t mag = 0u;

    memset(&tmp[0], 0, sizeof(tmp));
    if (v < 0) {
        mag = (uint64_t)(-v);
    } else {
        mag = (uint64_t)v;
    }
    do {
        tmp[i] = (wchar_t)(L'0' + (wchar_t)(mag % 10u));
        i++;
        mag /= 10u;
    } while (mag != 0u);
    {
        uint32_t j = 0u;

        if (v < 0) {
            p_out[j] = L'-';
            j++;
        }
        while (i > 0u) {
            i--;
            p_out[j] = tmp[i];
            j++;
        }
        p_out[j] = L'\0';
    }
}

/**
 * @brief   小数部分手工补零(避免wsprintfW不支持'*'宽度)
 * @param   p_frac [out] 输出缓冲(至少8宽字符)
 * @param   frac_scaled 已换算的小数整数
 * @param   digits 目标位数(1~6)
 * @return  无
 */
static void fmt_frac_pad(wchar_t *p_frac, uint32_t frac_scaled, uint32_t digits)
{
    wchar_t tmp[12u];
    uint32_t len = 0u;
    uint32_t i = 0u;

    memset(&tmp[0], 0, sizeof(tmp));
    len = (uint32_t)wsprintfW(&tmp[0], L"%u", (unsigned int)frac_scaled);
    for (i = 0u; i < digits; i++) {
        p_frac[i] = L'0';
    }
    p_frac[digits] = L'\0';
    if (len <= digits) {
        memcpy(&p_frac[digits - len], &tmp[0], len * sizeof(wchar_t));
    } else {
        /* 超长(不应发生): 截断 */
        memcpy(&p_frac[0], &tmp[0], digits * sizeof(wchar_t));
    }
}

/**
 * @brief   拼接数字字段行: "名称: 原值 (0x..) = 换算值[ 单位]"
 * @param   p_line   [out] 行缓冲
 * @param   p_name   字段名
 * @param   raw      原始WORD值
 * @param   scale_milli 换算系数(千分表示: 100=×0.1, 1000=×1, 1=×0.001)
 * @param   offset   换算偏移(先减, 可为负)
 * @param   digits   小数位数(0~6)
 * @param   p_unit   单位(可为NULL)
 * @return  无
 */
static void fmt_word(wchar_t *p_line, const wchar_t *p_name, uint16_t raw,
                     int32_t scale_milli, int32_t offset, uint32_t digits,
                     const wchar_t *p_unit)
{
    if (raw == 0xFFFFu) {
        (void)wsprintfW(p_line, L"%s: 0xFFFF (无效)", p_name);
        return;
    }
    if (raw == 0xFFFEu) {
        (void)wsprintfW(p_line, L"%s: 0xFFFE (异常)", p_name);
        return;
    }
    {
        int64_t scaled = ((int64_t)(int32_t)raw - (int64_t)offset) * (int64_t)scale_milli;
        int64_t whole = scaled / 1000;
        int32_t frac = (int32_t)(scaled % 1000);
        uint32_t div = 1u;
        uint32_t i = 0u;
        wchar_t wbuf[24u];
        wchar_t fbuf[8u];

        if (frac < 0) {
            frac = -frac;
        }
        for (i = 0u; i < digits; i++) {
            div *= 10u;
        }
        w_i64(&wbuf[0], whole);

        if (digits == 0u) {
            if (p_unit != NULL) {
                (void)wsprintfW(p_line, L"%s: %u (0x%04X) = %s %s", p_name,
                                (unsigned int)raw, (unsigned int)raw, &wbuf[0], p_unit);
            } else {
                (void)wsprintfW(p_line, L"%s: %u (0x%04X) = %s", p_name,
                                (unsigned int)raw, (unsigned int)raw, &wbuf[0]);
            }
        } else {
            fmt_frac_pad(&fbuf[0], ((uint32_t)frac * div) / 1000u, digits);
            if (p_unit != NULL) {
                (void)wsprintfW(p_line, L"%s: %u (0x%04X) = %s.%s %s", p_name,
                                (unsigned int)raw, (unsigned int)raw, &wbuf[0],
                                &fbuf[0], p_unit);
            } else {
                (void)wsprintfW(p_line, L"%s: %u (0x%04X) = %s.%s", p_name,
                                (unsigned int)raw, (unsigned int)raw, &wbuf[0],
                                &fbuf[0]);
            }
        }
    }
}

/**
 * @brief   DWORD字段换算行(里程/转矩类, 64位有符号)
 * @param   p_line   [out] 行缓冲
 * @param   p_name   字段名
 * @param   raw      原始DWORD值
 * @param   scale_milli 换算系数(千分表示)
 * @param   offset   换算偏移
 * @param   digits   小数位数
 * @param   p_unit   单位
 * @return  无
 */
static void fmt_dword(wchar_t *p_line, const wchar_t *p_name, uint32_t raw,
                      int32_t scale_milli, int32_t offset, uint32_t digits,
                      const wchar_t *p_unit)
{
    if (raw == 0xFFFFFFFFu) {
        (void)wsprintfW(p_line, L"%s: 0xFFFFFFFF (无效)", p_name);
        return;
    }
    if (raw == 0xFFFFFFFEu) {
        (void)wsprintfW(p_line, L"%s: 0xFFFFFFFE (异常)", p_name);
        return;
    }
    {
        int64_t scaled = ((int64_t)raw - (int64_t)offset) * (int64_t)scale_milli;
        int64_t whole = scaled / 1000;
        int32_t frac = (int32_t)(scaled % 1000);
        uint32_t div = 1u;
        uint32_t i = 0u;
        wchar_t wbuf[24u];

        if (frac < 0) {
            frac = -frac;
        }
        for (i = 0u; i < digits; i++) {
            div *= 10u;
        }
        w_i64(&wbuf[0], whole);
        if (digits == 0u) {
            (void)wsprintfW(p_line, L"%s: %s %s", p_name, &wbuf[0],
                            (p_unit != NULL) ? p_unit : L"");
        } else {
            wchar_t fbuf[8u];

            fmt_frac_pad(&fbuf[0], ((uint32_t)frac * div) / 1000u, digits);
            (void)wsprintfW(p_line, L"%s: %s.%s %s", p_name, &wbuf[0], &fbuf[0],
                            (p_unit != NULL) ? p_unit : L"");
        }
    }
}

/**
 * @brief   BYTE温度行(偏移-40℃, 特殊值标注)
 * @param   p_line [out] 行缓冲
 * @param   p_name 字段名
 * @param   raw    BYTE原始值
 * @return  无
 */
static void fmt_temp(wchar_t *p_line, const wchar_t *p_name, uint8_t raw)
{
    if (raw == 0xFFu) {
        (void)wsprintfW(p_line, L"%s: 0xFF (无效)", p_name);
        return;
    }
    if (raw == 0xFEu) {
        (void)wsprintfW(p_line, L"%s: 0xFE (异常)", p_name);
        return;
    }
    (void)wsprintfW(p_line, L"%s: %u = %d ℃", p_name, (unsigned int)raw,
                    (int32_t)raw - 40);
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 枚举描述                                                     */
/* -------------------------------------------------------------------------- */

/**
 * @brief   命令标识描述
 * @param   v 命令值
 * @return  描述
 */
static const wchar_t *cmd_desc(uint8_t v)
{
    switch (v)
    {
        case 0x01u: return L"车辆登入";
        case 0x02u: return L"实时信息上报";
        case 0x03u: return L"补发信息上报";
        case 0x04u: return L"车辆登出";
        case 0x05u: return L"平台登入";
        case 0x06u: return L"平台登出";
        case 0x07u: return L"心跳";
        case 0x08u: return L"终端校时";
        case 0x09u: return L"激活";
        case 0x0Au: return L"激活应答";
        case 0x0Bu: return L"密钥交换";
        case 0x80u: return L"查询命令";
        case 0x81u: return L"设置命令";
        case 0x82u: return L"控制命令";
        default:    return L"未知";
    }
}

/**
 * @brief   信息类型描述
 * @param   v 类型值
 * @return  描述
 */
static const wchar_t *info_desc(uint8_t v)
{
    switch (v)
    {
        case 0x01u: return L"整车数据";
        case 0x02u: return L"驱动电机数据";
        case 0x03u: return L"燃料电池发动机及车载氢系统数据";
        case 0x04u: return L"发动机数据";
        case 0x05u: return L"车辆位置数据";
        case 0x06u: return L"报警数据";
        case 0x07u: return L"动力蓄电池最小并联单元电压数据";
        case 0x08u: return L"动力蓄电池温度数据";
        case 0x30u: return L"燃料电池电堆数据";
        case 0x31u: return L"超级电容器数据";
        case 0x32u: return L"超级电容器极值数据";
        case 0xFFu: return L"签名数据";
        default:    return L"自定义数据";
    }
}

/**
 * @brief   加密方式描述
 * @param   v 加密值
 * @return  描述
 */
static const wchar_t *enc_desc(uint8_t v)
{
    switch (v)
    {
        case 0x01u: return L"不加密";
        case 0x02u: return L"RSA加密";
        case 0x03u: return L"AES加密";
        case 0x04u: return L"SM2加密";
        case 0x05u: return L"SM4加密";
        case 0xFEu: return L"异常";
        case 0xFFu: return L"无效";
        default:    return L"未知";
    }
}

/**
 * @brief   应答标志描述
 * @param   v 应答值
 * @return  描述
 */
static const wchar_t *resp_desc(uint8_t v)
{
    switch (v)
    {
        case 0x01u: return L"成功";
        case 0x02u: return L"其他错误";
        case 0x03u: return L"VIN重复";
        case 0x04u: return L"VIN不存在";
        case 0x05u: return L"验签错误";
        case 0x06u: return L"数据结构错误";
        case 0x07u: return L"解密错误";
        case 0xFEu: return L"命令包";
        default:    return L"未知";
    }
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 信息体解析                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   解析整车数据(0x01)
 * @param   p_cur   游标
 * @param   p_ctx   输出
 * @param   indent  缩进
 * @return  无
 */
static void parse_vehicle(GB_CUR_T *p_cur, GB_OUT_T *p_ctx, uint32_t indent)
{
    wchar_t line[GB_LINE_MAX];
    uint8_t st = rd_u8(p_cur);
    uint8_t chg = rd_u8(p_cur);
    uint8_t mode = rd_u8(p_cur);
    uint16_t speed = rd_u16(p_cur);
    uint32_t mileage = rd_u32(p_cur);
    uint16_t volt = rd_u16(p_cur);
    uint16_t curr = rd_u16(p_cur);
    uint8_t soc = rd_u8(p_cur);
    uint8_t dcdc = rd_u8(p_cur);
    uint8_t gear = rd_u8(p_cur);
    uint16_t ins_res = rd_u16(p_cur);

    (void)wsprintfW(line, L"车辆状态: 0x%02X", (unsigned int)st);
    out_line(p_ctx, indent, line);
    (void)wsprintfW(line, L"充电状态: 0x%02X", (unsigned int)chg);
    out_line(p_ctx, indent, line);
    (void)wsprintfW(line, L"运行模式: 0x%02X", (unsigned int)mode);
    out_line(p_ctx, indent, line);
    fmt_word(line, L"车速", speed, 100, 0, 1, L"km/h");
    out_line(p_ctx, indent, line);
    fmt_dword(line, L"累计里程", mileage, 100, 0, 1, L"km");
    out_line(p_ctx, indent, line);
    fmt_word(line, L"总电压", volt, 100, 0, 1, L"V");
    out_line(p_ctx, indent, line);
    fmt_word(line, L"总电流", curr, 100, 30000, 1, L"A");
    out_line(p_ctx, indent, line);
    (void)wsprintfW(line, L"SOC: %u%%", (unsigned int)soc);
    out_line(p_ctx, indent, line);
    (void)wsprintfW(line, L"DC-DC状态: 0x%02X", (unsigned int)dcdc);
    out_line(p_ctx, indent, line);
    (void)wsprintfW(line, L"挡位: 0x%02X", (unsigned int)gear);
    out_line(p_ctx, indent, line);
    fmt_word(line, L"绝缘电阻", ins_res, 1, 0, 0, L"kΩ");
    out_line(p_ctx, indent, line);
}

/**
 * @brief   解析驱动电机数据(0x02)
 * @param   p_cur   游标
 * @param   p_ctx   输出
 * @param   indent  缩进
 * @return  无
 */
static void parse_motors(GB_CUR_T *p_cur, GB_OUT_T *p_ctx, uint32_t indent)
{
    wchar_t line[GB_LINE_MAX];
    uint8_t count = rd_u8(p_cur);
    uint32_t i = 0u;

    (void)wsprintfW(line, L"驱动电机个数: %u", (unsigned int)count);
    out_line(p_ctx, indent, line);
    for (i = 0u; (i < count) && (i < GB_LIST_MAX); i++) {
        uint8_t id = rd_u8(p_cur);
        uint8_t st = rd_u8(p_cur);
        uint8_t ctl_temp = rd_u8(p_cur);
        uint16_t speed = rd_u16(p_cur);
        uint32_t torque = rd_u32(p_cur);
        uint8_t motor_temp = rd_u8(p_cur);

        (void)wsprintfW(line, L"— 电机%u —", (unsigned int)(i + 1u));
        out_line(p_ctx, indent, line);
        (void)wsprintfW(line, L"电机编号: %u  状态: 0x%02X",
                        (unsigned int)id, (unsigned int)st);
        out_line(p_ctx, indent + 1u, line);
        fmt_temp(line, L"控制器温度", ctl_temp);
        out_line(p_ctx, indent + 1u, line);
        fmt_word(line, L"转速", speed, 1, 32000, 0, L"r/min");
        out_line(p_ctx, indent + 1u, line);
        fmt_dword(line, L"转矩", torque, 100, 200000, 1, L"N·m");
        out_line(p_ctx, indent + 1u, line);
        fmt_temp(line, L"电机温度", motor_temp);
        out_line(p_ctx, indent + 1u, line);
    }
}

/**
 * @brief   解析燃料电池数据(0x03)
 * @param   p_cur   游标
 * @param   p_ctx   输出
 * @param   indent  缩进
 * @return  无
 */
static void parse_fuel_cell(GB_CUR_T *p_cur, GB_OUT_T *p_ctx, uint32_t indent)
{
    wchar_t line[GB_LINE_MAX];
    uint16_t temp = rd_u16(p_cur);
    uint8_t probe = rd_u8(p_cur);
    uint16_t conc = rd_u16(p_cur);
    uint8_t sensor = rd_u8(p_cur);
    uint16_t press = rd_u16(p_cur);
    uint8_t press_sensor = rd_u8(p_cur);
    uint8_t dcdc = rd_u8(p_cur);
    uint8_t remain = rd_u8(p_cur);
    uint8_t ctl_temp = rd_u8(p_cur);

    fmt_word(line, L"氢系统最高温度", temp, 1, 40, 0, L"℃");
    out_line(p_ctx, indent, line);
    (void)wsprintfW(line, L"最高温度探针序号: %u", (unsigned int)probe);
    out_line(p_ctx, indent, line);
    if (conc == 0xFFFFu) {
        out_line(p_ctx, indent, L"氢气最高浓度: 0xFFFF (无效)");
    } else if (conc == 0xFFFEu) {
        out_line(p_ctx, indent, L"氢气最高浓度: 0xFFFE (异常)");
    } else {
        (void)wsprintfW(line, L"氢气最高浓度: %u = %u.%04u %%", (unsigned int)conc,
                        (unsigned int)(conc / 10000u), (unsigned int)(conc % 10000u));
        out_line(p_ctx, indent, line);
    }
    (void)wsprintfW(line, L"最高浓度传感器序号: %u", (unsigned int)sensor);
    out_line(p_ctx, indent, line);
    fmt_word(line, L"氢气最高压力", press, 100, 0, 1, L"MPa");
    out_line(p_ctx, indent, line);
    (void)wsprintfW(line, L"最高压力传感器序号: %u", (unsigned int)press_sensor);
    out_line(p_ctx, indent, line);
    (void)wsprintfW(line, L"DC-DC状态: 0x%02X  剩余氢量: %u%%",
                    (unsigned int)dcdc, (unsigned int)remain);
    out_line(p_ctx, indent, line);
    fmt_temp(line, L"DC-DC控制器温度", ctl_temp);
    out_line(p_ctx, indent, line);
}

/**
 * @brief   解析发动机数据(0x04)
 * @param   p_cur   游标
 * @param   p_ctx   输出
 * @param   indent  缩进
 * @return  无
 */
static void parse_engine(GB_CUR_T *p_cur, GB_OUT_T *p_ctx, uint32_t indent)
{
    wchar_t line[GB_LINE_MAX];
    uint16_t speed = rd_u16(p_cur);

    (void)wsprintfW(line, L"曲轴转速: %u r/min", (unsigned int)speed);
    out_line(p_ctx, indent, line);
}

/**
 * @brief   解析车辆位置数据(0x05)
 * @param   p_cur   游标
 * @param   p_ctx   输出
 * @param   indent  缩进
 * @return  无
 */
static void parse_location(GB_CUR_T *p_cur, GB_OUT_T *p_ctx, uint32_t indent)
{
    wchar_t line[GB_LINE_MAX];
    uint8_t status = rd_u8(p_cur);
    uint8_t coord = rd_u8(p_cur);
    uint32_t lon = rd_u32(p_cur);
    uint32_t lat = rd_u32(p_cur);
    const wchar_t *valid = ((status & 0x01u) == 0u) ? L"有效" : L"无效";
    const wchar_t *ns = ((status & 0x02u) != 0u) ? L"南纬" : L"北纬";
    const wchar_t *ew = ((status & 0x04u) != 0u) ? L"西经" : L"东经";

    (void)wsprintfW(line, L"定位状态: 0x%02X (%s, %s, %s)",
                    (unsigned int)status, valid, ns, ew);
    out_line(p_ctx, indent, line);
    (void)wsprintfW(line, L"坐标系: 0x%02X", (unsigned int)coord);
    out_line(p_ctx, indent, line);
    if (lon == 0xFFFFFFFFu) {
        out_line(p_ctx, indent, L"经度: 0xFFFFFFFF (无效)");
    } else if (lon == 0xFFFFFFFEu) {
        out_line(p_ctx, indent, L"经度: 0xFFFFFFFE (异常)");
    } else {
        (void)wsprintfW(line, L"经度: %u.%06u°", (unsigned int)(lon / 1000000u),
                        (unsigned int)(lon % 1000000u));
        out_line(p_ctx, indent, line);
    }
    if (lat == 0xFFFFFFFFu) {
        out_line(p_ctx, indent, L"纬度: 0xFFFFFFFF (无效)");
    } else if (lat == 0xFFFFFFFEu) {
        out_line(p_ctx, indent, L"纬度: 0xFFFFFFFE (异常)");
    } else {
        (void)wsprintfW(line, L"纬度: %u.%06u°", (unsigned int)(lat / 1000000u),
                        (unsigned int)(lat % 1000000u));
        out_line(p_ctx, indent, line);
    }
}

/**
 * @brief   解析报警数据(0x06)
 * @param   p_cur   游标
 * @param   p_ctx   输出
 * @param   indent  缩进
 * @return  无
 */
static void parse_alarm(GB_CUR_T *p_cur, GB_OUT_T *p_ctx, uint32_t indent)
{
    static const wchar_t *const alarm_names[28u] =
    {
        L"温度差异报警", L"电池高温报警", L"储能装置过压报警", L"储能装置欠压报警",
        L"SOC低报警", L"单体过压报警", L"单体欠压报警", L"SOC过高报警",
        L"SOC跳变报警", L"储能系统不匹配报警", L"单体一致性差报警", L"绝缘电阻失效报警",
        L"DC-DC温度报警", L"制动系统报警", L"DC-DC状态报警", L"电机控制器温度报警",
        L"高压互锁状态报警", L"驱动电机温度报警", L"储能装置过充报警", L"驱动电机超速报警",
        L"驱动电机过流报警", L"超级电容过温报警", L"超级电容过压报警", L"储能装置热事件报警",
        L"氢气泄漏异常报警", L"氢系统压力异常报警", L"氢系统温度异常报警", L"燃料电池电堆超温报警"
    };
    wchar_t line[GB_LINE_MAX];
    uint8_t level = rd_u8(p_cur);
    uint32_t flags = rd_u32(p_cur);
    uint8_t batt_n = rd_u8(p_cur);
    uint8_t motor_n = rd_u8(p_cur);
    uint8_t eng_n = rd_u8(p_cur);
    uint8_t other_n = rd_u8(p_cur);
    uint8_t general_n = rd_u8(p_cur);
    uint32_t i = 0u;
    uint32_t bit = 0u;
    uint32_t alarm_cnt = 0u;

    (void)wsprintfW(line, L"最高报警等级: %u", (unsigned int)level);
    out_line(p_ctx, indent, line);
    (void)wsprintfW(line, L"通用报警标志: 0x%08X", (unsigned int)flags);
    out_line(p_ctx, indent, line);
    for (bit = 0u; bit < 28u; bit++) {
        if (((flags >> bit) & 0x01u) != 0u) {
            (void)wsprintfW(line, L"▲ [bit%u] %s", (unsigned int)bit, alarm_names[bit]);
            out_line(p_ctx, indent + 1u, line);
            alarm_cnt++;
        }
    }
    if (alarm_cnt == 0u) {
        out_line(p_ctx, indent + 1u, L"(无通用报警)");
    }
    (void)wsprintfW(line, L"可充电储能装置故障数: %u", (unsigned int)batt_n);
    out_line(p_ctx, indent, line);
    for (i = 0u; (i < batt_n) && (i < GB_LIST_MAX); i++) {
        (void)wsprintfW(line, L"故障码%u: 0x%08X", (unsigned int)(i + 1u),
                        (unsigned int)rd_u32(p_cur));
        out_line(p_ctx, indent + 1u, line);
    }
    (void)wsprintfW(line, L"驱动电机故障数: %u", (unsigned int)motor_n);
    out_line(p_ctx, indent, line);
    for (i = 0u; (i < motor_n) && (i < GB_LIST_MAX); i++) {
        (void)wsprintfW(line, L"故障码%u: 0x%08X", (unsigned int)(i + 1u),
                        (unsigned int)rd_u32(p_cur));
        out_line(p_ctx, indent + 1u, line);
    }
    (void)wsprintfW(line, L"发动机故障数: %u", (unsigned int)eng_n);
    out_line(p_ctx, indent, line);
    for (i = 0u; (i < eng_n) && (i < GB_LIST_MAX); i++) {
        (void)wsprintfW(line, L"故障码%u: 0x%08X", (unsigned int)(i + 1u),
                        (unsigned int)rd_u32(p_cur));
        out_line(p_ctx, indent + 1u, line);
    }
    (void)wsprintfW(line, L"其他故障数: %u", (unsigned int)other_n);
    out_line(p_ctx, indent, line);
    for (i = 0u; (i < other_n) && (i < GB_LIST_MAX); i++) {
        (void)wsprintfW(line, L"故障码%u: 0x%08X", (unsigned int)(i + 1u),
                        (unsigned int)rd_u32(p_cur));
        out_line(p_ctx, indent + 1u, line);
    }
    (void)wsprintfW(line, L"通用报警故障数: %u", (unsigned int)general_n);
    out_line(p_ctx, indent, line);
    for (i = 0u; (i < general_n) && (i < GB_LIST_MAX); i++) {
        uint8_t idx = rd_u8(p_cur);
        uint8_t lv = rd_u8(p_cur);

        (void)wsprintfW(line, L"[bit%u] 等级%u", (unsigned int)idx, (unsigned int)lv);
        out_line(p_ctx, indent + 1u, line);
    }
}

/**
 * @brief   解析动力蓄电池电压数据(0x07)
 * @param   p_cur   游标
 * @param   p_ctx   输出
 * @param   indent  缩进
 * @return  无
 */
static void parse_batt_volt(GB_CUR_T *p_cur, GB_OUT_T *p_ctx, uint32_t indent)
{
    wchar_t line[GB_LINE_MAX];
    uint8_t pack_count = rd_u8(p_cur);
    uint32_t i = 0u;
    uint32_t j = 0u;

    (void)wsprintfW(line, L"电池包总数: %u", (unsigned int)pack_count);
    out_line(p_ctx, indent, line);
    for (i = 0u; (i < pack_count) && (i < GB_LIST_MAX); i++) {
        uint8_t pack_id = rd_u8(p_cur);
        uint16_t pack_v = rd_u16(p_cur);
        uint16_t pack_i = rd_u16(p_cur);
        uint16_t unit_n = rd_u16(p_cur);

        (void)wsprintfW(line, L"— 电池包%u (序号%u) —",
                        (unsigned int)(i + 1u), (unsigned int)pack_id);
        out_line(p_ctx, indent, line);
        fmt_word(line, L"电池包电压", pack_v, 100, 0, 1, L"V");
        out_line(p_ctx, indent + 1u, line);
        fmt_word(line, L"电池包电流", pack_i, 100, 30000, 1, L"A");
        out_line(p_ctx, indent + 1u, line);
        (void)wsprintfW(line, L"最小并联单元总数: %u", (unsigned int)unit_n);
        out_line(p_ctx, indent + 1u, line);
        {
            wchar_t row[GB_LINE_MAX];
            uint32_t col = 0u;

            memset(&row[0], 0, sizeof(row));
            for (j = 0u; (j < unit_n) && (j < (GB_LIST_MAX * 16u)); j++) {
                uint16_t uv = rd_u16(p_cur);
                wchar_t cell[24u];

                if (uv == 0xFFFFu) {
                    (void)wsprintfW(&cell[0], L"[无效] ");
                } else if (uv == 0xFFFEu) {
                    (void)wsprintfW(&cell[0], L"[异常] ");
                } else {
                    (void)wsprintfW(&cell[0], L"%u.%03uV ", (unsigned int)(uv / 1000u),
                                    (unsigned int)(uv % 1000u));
                }
                (void)lstrcatW(&row[0], &cell[0]);
                col++;
                if ((col >= 8u) || ((j + 1u) == unit_n)) {
                    out_line(p_ctx, indent + 2u, &row[0]);
                    memset(&row[0], 0, sizeof(row));
                    col = 0u;
                }
            }
        }
    }
}

/**
 * @brief   解析动力蓄电池温度数据(0x08)
 * @param   p_cur   游标
 * @param   p_ctx   输出
 * @param   indent  缩进
 * @return  无
 */
static void parse_batt_temp(GB_CUR_T *p_cur, GB_OUT_T *p_ctx, uint32_t indent)
{
    wchar_t line[GB_LINE_MAX];
    uint8_t pack_count = rd_u8(p_cur);
    uint32_t i = 0u;
    uint32_t j = 0u;

    (void)wsprintfW(line, L"电池包总数: %u", (unsigned int)pack_count);
    out_line(p_ctx, indent, line);
    for (i = 0u; (i < pack_count) && (i < GB_LIST_MAX); i++) {
        uint8_t pack_id = rd_u8(p_cur);
        uint16_t probe_n = rd_u16(p_cur);
        wchar_t row[GB_LINE_MAX];

        (void)wsprintfW(line, L"— 电池包%u (序号%u) 探针数:%u —",
                        (unsigned int)(i + 1u), (unsigned int)pack_id,
                        (unsigned int)probe_n);
        out_line(p_ctx, indent, line);
        memset(&row[0], 0, sizeof(row));
        for (j = 0u; (j < probe_n) && (j < (GB_LIST_MAX * 8u)); j++) {
            uint8_t t = rd_u8(p_cur);
            wchar_t cell[16u];

            if (t == 0xFFu) {
                (void)wsprintfW(&cell[0], L"[无效] ");
            } else if (t == 0xFEu) {
                (void)wsprintfW(&cell[0], L"[异常] ");
            } else {
                (void)wsprintfW(&cell[0], L"%d℃ ", (int32_t)t - 40);
            }
            (void)lstrcatW(&row[0], &cell[0]);
            if (((j % 12u) == 11u) || ((j + 1u) == probe_n)) {
                out_line(p_ctx, indent + 1u, &row[0]);
                memset(&row[0], 0, sizeof(row));
            }
        }
    }
}

/**
 * @brief   按信息类型分发解析(未知类型展示剩余原始hex)
 * @param   p_cur   游标
 * @param   p_ctx   输出
 * @param   indent  缩进
 * @return  无
 */
static void parse_info_body(GB_CUR_T *p_cur, GB_OUT_T *p_ctx, uint32_t indent)
{
    uint8_t info_type = rd_u8(p_cur);
    wchar_t line[GB_LINE_MAX];

    (void)wsprintfW(line, L"◆ %s (0x%02X)", info_desc(info_type), (unsigned int)info_type);
    out_line(p_ctx, indent, line);

    switch (info_type)
    {
        case 0x01u: parse_vehicle(p_cur, p_ctx, indent + 1u); break;
        case 0x02u: parse_motors(p_cur, p_ctx, indent + 1u); break;
        case 0x03u: parse_fuel_cell(p_cur, p_ctx, indent + 1u); break;
        case 0x04u: parse_engine(p_cur, p_ctx, indent + 1u); break;
        case 0x05u: parse_location(p_cur, p_ctx, indent + 1u); break;
        case 0x06u: parse_alarm(p_cur, p_ctx, indent + 1u); break;
        case 0x07u: parse_batt_volt(p_cur, p_ctx, indent + 1u); break;
        case 0x08u: parse_batt_temp(p_cur, p_ctx, indent + 1u); break;
        default:
        {
            uint32_t remain = p_cur->len - p_cur->off;
            uint32_t show = (remain > 64u) ? 64u : remain;
            uint32_t i = 0u;

            (void)wsprintfW(line, L"原始数据(前%u/%u字节):", (unsigned int)show,
                            (unsigned int)remain);
            out_line(p_ctx, indent + 1u, line);
            memset(&line[0], 0, sizeof(line));
            for (i = 0u; i < show; i++) {
                wchar_t hex[4u];

                (void)wsprintfW(&hex[0], L"%02X", (unsigned int)rd_u8(p_cur));
                (void)lstrcatW(&line[0], &hex[0]);
                if (((i % 32u) == 31u) || ((i + 1u) == show)) {
                    out_line(p_ctx, indent + 2u, &line[0]);
                    memset(&line[0], 0, sizeof(line));
                }
            }
            break;
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 数据单元解析                                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief   输出时间字段行
 * @param   p_cur   游标(读6字节时间)
 * @param   p_ctx   输出
 * @param   indent  缩进
 * @return  无
 */
static void out_time(GB_CUR_T *p_cur, GB_OUT_T *p_ctx, uint32_t indent)
{
    wchar_t line[GB_LINE_MAX];
    uint8_t yy = rd_u8(p_cur);
    uint8_t mm = rd_u8(p_cur);
    uint8_t dd = rd_u8(p_cur);
    uint8_t hh = rd_u8(p_cur);
    uint8_t mi = rd_u8(p_cur);
    uint8_t ss = rd_u8(p_cur);

    (void)wsprintfW(line, L"时间: 20%02u-%02u-%02u %02u:%02u:%02u",
                    (unsigned int)yy, (unsigned int)mm, (unsigned int)dd,
                    (unsigned int)hh, (unsigned int)mi, (unsigned int)ss);
    out_line(p_ctx, indent, line);
}

/**
 * @brief   解析数据单元(按命令标识分发)
 * @param   p_cur   游标(位于数据单元首)
 * @param   p_ctx   输出
 * @param   cmd     命令标识
 * @param   resp    应答标志
 * @return  无
 */
static void parse_data_unit(GB_CUR_T *p_cur, GB_OUT_T *p_ctx, uint8_t cmd, uint8_t resp)
{
    uint32_t indent = 1u;
    wchar_t line[GB_LINE_MAX];

    if (resp != 0xFEu) {
        (void)wsprintfW(line, L"应答: 0x%02X (%s)", (unsigned int)resp, resp_desc(resp));
        out_line(p_ctx, indent, line);
        return;
    }

    switch (cmd)
    {
        case 0x01u:   /* 车辆登入 */
        {
            wchar_t txt[64u];

            out_time(p_cur, p_ctx, indent);
            (void)wsprintfW(line, L"登入流水号: %u", (unsigned int)rd_u16(p_cur));
            out_line(p_ctx, indent, line);
            rd_str(p_cur, &txt[0], 64u, 20u);
            (void)wsprintfW(line, L"ICCID: %s", &txt[0]);
            out_line(p_ctx, indent, line);
            {
                uint8_t bms_n = rd_u8(p_cur);
                uint32_t i = 0u;
                uint32_t total = 0u;

                (void)wsprintfW(line, L"BMS数: %u", (unsigned int)bms_n);
                out_line(p_ctx, indent, line);
                for (i = 0u; (i < bms_n) && (i < GB_LIST_MAX); i++) {
                    uint8_t n = rd_u8(p_cur);

                    total += n;
                    (void)wsprintfW(line, L"BMS%u 电池包数: %u",
                                    (unsigned int)(i + 1u), (unsigned int)n);
                    out_line(p_ctx, indent + 1u, line);
                }
                (void)wsprintfW(line, L"电池包编码(%u个):", (unsigned int)total);
                out_line(p_ctx, indent, line);
                for (i = 0u; (i < total) && (i < (GB_LIST_MAX * 4u)); i++) {
                    rd_str(p_cur, &txt[0], 64u, 24u);
                    (void)wsprintfW(line, L"包%u: %s", (unsigned int)(i + 1u), &txt[0]);
                    out_line(p_ctx, indent + 1u, line);
                }
            }
            break;
        }
        case 0x04u:   /* 车辆登出 */
        {
            out_time(p_cur, p_ctx, indent);
            (void)wsprintfW(line, L"登出流水号: %u", (unsigned int)rd_u16(p_cur));
            out_line(p_ctx, indent, line);
            break;
        }
        case 0x05u:   /* 平台登入 */
        {
            wchar_t txt[64u];

            out_time(p_cur, p_ctx, indent);
            (void)wsprintfW(line, L"登入流水号: %u", (unsigned int)rd_u16(p_cur));
            out_line(p_ctx, indent, line);
            rd_str(p_cur, &txt[0], 64u, 12u);
            (void)wsprintfW(line, L"平台用户名: %s", &txt[0]);
            out_line(p_ctx, indent, line);
            rd_str(p_cur, &txt[0], 64u, 20u);
            (void)wsprintfW(line, L"平台密码: %s", &txt[0]);
            out_line(p_ctx, indent, line);
            (void)wsprintfW(line, L"加密规则: 0x%02X", (unsigned int)rd_u8(p_cur));
            out_line(p_ctx, indent, line);
            break;
        }
        case 0x06u:   /* 平台登出 */
        {
            out_time(p_cur, p_ctx, indent);
            (void)wsprintfW(line, L"登出流水号: %u", (unsigned int)rd_u16(p_cur));
            out_line(p_ctx, indent, line);
            break;
        }
        case 0x02u:
        case 0x03u:   /* 实时/补发上报 */
        {
            out_time(p_cur, p_ctx, indent);
            while ((p_cur->off < p_cur->len) && (p_cur->truncated == 0u)) {
                parse_info_body(p_cur, p_ctx, indent);
            }
            break;
        }
        case 0x07u:   /* 心跳: 数据单元为空 */
        {
            out_line(p_ctx, indent, L"(心跳, 数据单元为空)");
            break;
        }
        default:
        {
            uint32_t remain = p_cur->len - p_cur->off;
            uint32_t i = 0u;

            (void)wsprintfW(line, L"原始数据(%u字节):", (unsigned int)remain);
            out_line(p_ctx, indent, line);
            memset(&line[0], 0, sizeof(line));
            for (i = 0u; i < remain; i++) {
                wchar_t hex[4u];

                (void)wsprintfW(&hex[0], L"%02X", (unsigned int)rd_u8(p_cur));
                (void)lstrcatW(&line[0], &hex[0]);
                if (((i % 32u) == 31u) || ((i + 1u) == remain)) {
                    out_line(p_ctx, indent + 1u, &line[0]);
                    memset(&line[0], 0, sizeof(line));
                }
            }
            break;
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 单帧解析                                                     */
/* -------------------------------------------------------------------------- */

/**
 * @brief   解析单帧并输出报告
 * @param   p_frame   [in] 帧数据(含帧头与校验)
 * @param   frame_len 帧长度
 * @param   frame_idx 帧序号(1起)
 * @param   p_ctx     输出
 * @return  1: 成功; 0: 失败(报告已含原因)
 */
static uint8_t parse_frame(const uint8_t *p_frame, uint32_t frame_len,
                           uint32_t frame_idx, GB_OUT_T *p_ctx)
{
    wchar_t line[GB_LINE_MAX];
    wchar_t vin[GB_VIN_LEN + 2u];
    uint8_t cmd = 0u;
    uint8_t resp = 0u;
    uint8_t enc = 0u;
    uint16_t data_len = 0u;
    uint32_t bcc_calc = 0u;
    uint32_t i = 0u;

    (void)wsprintfW(line, L"══ 第 %u 帧 (%u字节) ══", (unsigned int)frame_idx,
                    (unsigned int)frame_len);
    out_line(p_ctx, 0u, line);

    if ((p_frame[0] == 0x23u) && (p_frame[1] == 0x23u)) {
        out_line(p_ctx, 1u, L"起始符: 0x2323 (## / 2016版)");
    } else if ((p_frame[0] == 0x24u) && (p_frame[1] == 0x24u)) {
        out_line(p_ctx, 1u, L"起始符: 0x2424 ($$ / 2025版)");
    } else {
        (void)wsprintfW(line, L"✗ 起始符: 0x%02X%02X (无效, 期望 2323/2424)",
                        (unsigned int)p_frame[0], (unsigned int)p_frame[1]);
        out_line(p_ctx, 1u, line);
        return 0u;
    }

    cmd = p_frame[2];
    resp = p_frame[3];
    memset(&vin[0], 0, sizeof(vin));
    for (i = 0u; i < GB_VIN_LEN; i++) {
        uint8_t ch = p_frame[4u + i];

        vin[i] = ((ch >= 0x20u) && (ch < 0x7Fu)) ? (wchar_t)ch : L'.';
    }
    enc = p_frame[21u];
    data_len = (uint16_t)(((uint32_t)p_frame[22u] << 8) | (uint32_t)p_frame[23u]);

    (void)wsprintfW(line, L"命令标识: 0x%02X (%s)", (unsigned int)cmd, cmd_desc(cmd));
    out_line(p_ctx, 1u, line);
    (void)wsprintfW(line, L"应答标志: 0x%02X (%s)", (unsigned int)resp, resp_desc(resp));
    out_line(p_ctx, 1u, line);
    (void)wsprintfW(line, L"VIN: %s", &vin[0]);
    out_line(p_ctx, 1u, line);
    (void)wsprintfW(line, L"加密方式: 0x%02X (%s)", (unsigned int)enc, enc_desc(enc));
    out_line(p_ctx, 1u, line);
    (void)wsprintfW(line, L"数据单元长度: %u 字节", (unsigned int)data_len);
    out_line(p_ctx, 1u, line);

    if (frame_len < (GB_HDR_LEN + (uint32_t)data_len + 1u)) {
        (void)wsprintfW(line, L"✗ 数据不足: 需 %u 字节, 实际 %u 字节(可能被截断)",
                        (unsigned int)(GB_HDR_LEN + (uint32_t)data_len + 1u),
                        (unsigned int)frame_len);
        out_line(p_ctx, 1u, line);
        return 0u;
    }

    for (i = 2u; i < (GB_HDR_LEN + (uint32_t)data_len); i++) {
        bcc_calc ^= (uint32_t)p_frame[i];
    }
    if ((uint8_t)bcc_calc != p_frame[GB_HDR_LEN + (uint32_t)data_len]) {
        (void)wsprintfW(line, L"✗ 校验码错误: 计算值 0x%02X, 接收值 0x%02X",
                        (unsigned int)bcc_calc,
                        (unsigned int)p_frame[GB_HDR_LEN + (uint32_t)data_len]);
        out_line(p_ctx, 1u, line);
        return 0u;
    }
    (void)wsprintfW(line, L"校验码: 0x%02X ✓ 通过",
                    (unsigned int)p_frame[GB_HDR_LEN + (uint32_t)data_len]);
    out_line(p_ctx, 1u, line);

    {
        GB_CUR_T cur;

        cur.p_data = &p_frame[GB_HDR_LEN];
        cur.len = (uint32_t)data_len;
        cur.off = 0u;
        cur.truncated = 0u;
        parse_data_unit(&cur, p_ctx, cmd, resp);
        if (cur.truncated != 0u) {
            out_line(p_ctx, 1u, L"⚠ 数据单元不完整(部分字段缺失)");
        }
    }

    return 1u;
}

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

uint32_t gb32960_parse_text(const wchar_t *p_hex_text, wchar_t *p_out, uint32_t out_cap)
{
    GB_OUT_T ctx;
    uint8_t frame[GB32960_FRAME_MAX];
    uint32_t byte_cnt = 0u;
    uint32_t frame_start = 0u;
    uint32_t frame_idx = 0u;
    uint32_t ok_frames = 0u;
    uint32_t text_len = 0u;
    uint32_t hex_pos = 0u;
    uint8_t hi = 0u;
    uint8_t have_hi = 0u;
    uint32_t i = 0u;

    if ((p_hex_text == NULL) || (p_out == NULL) || (out_cap < 64u)) {
        return 0u;
    }
    memset(&ctx, 0, sizeof(ctx));
    ctx.p_buf = p_out;
    ctx.cap = out_cap;
    p_out[0] = L'\0';
    text_len = (uint32_t)lstrlenW(p_hex_text);

    /* hex文本 -> 字节流(容错: 忽略空白/逗号/冒号/分号, 0x前缀) */
    for (hex_pos = 0u; hex_pos < text_len; hex_pos++) {
        wchar_t ch = p_hex_text[hex_pos];
        uint8_t val = 0u;
        uint8_t is_hex = 1u;

        if ((ch >= L'0') && (ch <= L'9')) {
            val = (uint8_t)(ch - L'0');
        } else if ((ch >= L'A') && (ch <= L'F')) {
            val = (uint8_t)(ch - L'A' + 10u);
        } else if ((ch >= L'a') && (ch <= L'f')) {
            val = (uint8_t)(ch - L'a' + 10u);
        } else if ((ch == L'x') || (ch == L'X')) {
            is_hex = 0u;   /* 0x前缀的x忽略(前导0已成半字节, 丢弃) */
            if (have_hi != 0u) {
                have_hi = 0u;
            }
        } else {
            is_hex = 0u;   /* 分隔符等忽略 */
        }

        if (is_hex != 0u) {
            if (have_hi == 0u) {
                hi = val;
                have_hi = 1u;
            } else {
                if (byte_cnt < GB32960_FRAME_MAX) {
                    frame[byte_cnt] = (uint8_t)((hi << 4) | val);
                }
                byte_cnt++;
                have_hi = 0u;
            }
        }
    }
    if (have_hi != 0u) {
        if (byte_cnt < GB32960_FRAME_MAX) {
            frame[byte_cnt] = (uint8_t)(hi << 4);
        }
        byte_cnt++;
    }
    if (byte_cnt > GB32960_FRAME_MAX) {
        (void)wsprintfW(p_out, L"✗ 输入过长: %u 字节 (上限 %u 字节)",
                        (unsigned int)byte_cnt, (unsigned int)GB32960_FRAME_MAX);
        return 0u;
    }
    if (byte_cnt < 26u) {
        (void)wsprintfW(p_out, L"✗ 数据过短: %u 字节 (最小26字节)",
                        (unsigned int)byte_cnt);
        return 0u;
    }

    /* 自动定位首个起始符(跳过前置噪声) */
    frame_start = 0u;
    while (((frame_start + 1u) < byte_cnt) &&
           !(((frame[frame_start] == 0x23u) && (frame[frame_start + 1u] == 0x23u)) ||
             ((frame[frame_start] == 0x24u) && (frame[frame_start + 1u] == 0x24u)))) {
        frame_start++;
    }
    if ((frame_start + 26u) > byte_cnt) {
        (void)lstrcpynW(p_out, L"✗ 未找到有效起始符(2323/2424)", (int)out_cap);
        return 0u;
    }

    /* 多帧连报: 逐帧切分解析 */
    i = frame_start;
    while ((i + 26u) <= byte_cnt) {
        uint16_t dlen = (uint16_t)(((uint32_t)frame[i + 22u] << 8) |
                                   (uint32_t)frame[i + 23u]);
        uint32_t flen = GB_HDR_LEN + (uint32_t)dlen + 1u;

        if ((i + flen) > byte_cnt) {
            wchar_t line[GB_LINE_MAX];

            frame_idx++;
            (void)wsprintfW(line, L"══ 第 %u 帧 (不完整) ══", (unsigned int)frame_idx);
            out_line(&ctx, 0u, line);
            (void)wsprintfW(line, L"✗ 需 %u 字节, 剩余 %u 字节",
                            (unsigned int)flen, (unsigned int)(byte_cnt - i));
            out_line(&ctx, 1u, line);
            break;
        }
        frame_idx++;
        if (parse_frame(&frame[i], flen, frame_idx, &ctx) != 0u) {
            ok_frames++;
            out_str(&ctx, L"\r\n");
            i += flen;
        } else {
            out_str(&ctx, L"\r\n");
            break;   /* 帧错误后停止切分(后续边界不可信) */
        }
    }
    if (frame_idx == 0u) {
        (void)lstrcpynW(p_out, L"✗ 无可解析的完整帧", (int)out_cap);
        return 0u;
    }

    return ok_frames;
}
