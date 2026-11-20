/**
 * @file    tool_manifest.c
 * @author  herongshun
 * @date    2026-11-05
 * @brief   集合版工具登记表实现
 *          新增工具两步登记: ① 在下方登记表中添加一行
 *                            ② 在根 build.bat 的源文件列表加入该工具模块源文件
 */

#include <stdint.h>
#include "tool_manifest.h"
#include "../tools/hexascii/tool_hexascii.h"
#include "../tools/dbgterm/tool_dbgterm.h"
#include "../tools/gb32960/tool_gb32960.h"

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define TOOL_ACCENT_BLUE   0x00F6823B  /**< 主题蓝 #3B82F6 */
#define TOOL_ACCENT_GREEN  0x005EC522  /**< 主题绿 #22C55E */
#define TOOL_ACCENT_PURPLE 0x00CC66AA  /**< 主题紫 #AA66CC */

/* -------------------------------------------------------------------------- */
/*  模块内部全局变量(只读登记表)                                                */
/* -------------------------------------------------------------------------- */

static const TOOL_INFO_T g_tool_manifest_table[] =
{
    {
        L"⇄",
        L"Hex ⇄ ASCII 转换",
        L"十六进制字符串与ASCII字符双向转换",
        TOOL_ACCENT_BLUE,
        tool_hexascii_open
    },
    {
        L"⇅",
        L"调试终端",
        L"ADB logcat接收·过滤·录制 (串口第二阶段)",
        TOOL_ACCENT_GREEN,
        tool_dbgterm_open
    },
    {
        L"❖",
        L"GB32960 解析",
        L"国标车辆报文解析 (2016/2025版, 多帧连报)",
        TOOL_ACCENT_PURPLE,
        tool_gb32960_open
    },
};

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   获取工具登记表
 * @param   p_count [out] 登记表内工具数量
 * @return  工具信息表首地址(只读); 入参非法时返回NULL
 */
const TOOL_INFO_T *tool_manifest_get_table(uint32_t *p_count)
{
    if (p_count == NULL) {
        return NULL;
    }

    *p_count = (uint32_t)(sizeof(g_tool_manifest_table) / sizeof(g_tool_manifest_table[0]));
    return g_tool_manifest_table;
}
