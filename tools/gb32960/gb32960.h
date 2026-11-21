/**
 * @file    gb32960.h
 * @author  herongshun
 * @date    2026-11-20
 * @brief   GB/T 32960.3-2025 协议解析核心接口(纯C, 无界面依赖)
 *          移植自 proto_master(PyQt)工程, 支持自动识别 2016(##)/2025($$) 版本、
 *          多帧连报、完整命令/信息类型解析与特殊值(异常/无效)标注;
 *          解析产出"节点树"(一行=一个节点, depth决定树/缩进层级),
 *          树形展示/缩进文本/记录重显共用同一份结构
 */

#ifndef GB32960_H
#define GB32960_H

#include <stdint.h>

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define GB32960_IN_MAX_WCHAR   32768u    /**< 输入hex文本宽字符容量 */
#define GB32960_FRAME_MAX      8192u     /**< 单帧/解码字节缓冲容量 */
#define GB32960_OUT_MAX_WCHAR  (160u * 1024u) /**< 解析报告输出容量(宽字符) */
#define GB32960_NODE_TEXT_WCHAR 288u     /**< 单节点行文本宽字符容量 */
#define GB32960_NODE_MAX        2048u    /**< 节点数组容量(超出截断并标注) */
#define GB32960_NODE_FLAG_FRAME 0x01u    /**< 节点标志: 帧标题行(记录/分隔依据) */
#define GB32960_FRAME_LIST_MAX  128u     /**< 多帧切分帧数上限 */

/* -------------------------------------------------------------------------- */
/*  类型定义                                                                    */
/* -------------------------------------------------------------------------- */

/** 解析结果节点(一行=一个节点) */
typedef struct GB32960_NODE_TAG
{
    uint8_t  depth;                            /**< 树深度/缩进层级(0=根) */
    uint8_t  flags;                            /**< 位标志(GB32960_NODE_FLAG_FRAME) */
    wchar_t  text[GB32960_NODE_TEXT_WCHAR];    /**< 行文本(已格式化) */
} GB32960_NODE_T;

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   解析hex文本并生成格式化解析报告(支持容错输入与多帧连报)
 *          容错: 自动忽略空格/制表/换行/逗号/冒号/0x前缀; 自动定位首个起始符;
 *          多帧: 按帧头+长度+校验逐帧切分连续解析
 * @param   p_hex_text [in]  hex文本(宽字符)
 * @param   p_out      [out] 解析报告输出缓冲
 * @param   out_cap    [in]  输出容量(宽字符)
 * @return  成功解析的帧数(0=全部失败, 报告含失败原因)
 */
uint32_t gb32960_parse_text(const wchar_t *p_hex_text, wchar_t *p_out, uint32_t out_cap);

/**
 * @brief   hex文本容错解码并按帧头长度切分多帧
 * @param   p_hex_text  [in]  hex文本(宽字符)
 * @param   p_bytes     [out] 解码字节缓冲(容量GB32960_FRAME_MAX)
 * @param   bytes_cap   [in]  字节缓冲容量
 * @param   p_offs      [out] 各帧起始偏移(相对p_bytes)
 * @param   p_lens      [out] 各帧长度(字节)
 * @param   frame_cap   [in]  帧偏移/长度数组容量
 * @param   p_err       [out] 失败原因文本(可为NULL)
 * @param   err_cap     [in]  失败原因容量
 * @return  切分出的帧数(含校验失败的帧; 0=失败, p_err含原因)
 */
uint32_t gb32960_decode_split(const wchar_t *p_hex_text, uint8_t *p_bytes, uint32_t bytes_cap,
                              uint32_t *p_offs, uint32_t *p_lens, uint32_t frame_cap,
                              wchar_t *p_err, uint32_t err_cap);

/**
 * @brief   解析单帧并追加到节点数组(记录重显/逐帧展示)
 * @param   p_frame   [in]     帧数据(含帧头与校验码)
 * @param   frame_len [in]     帧长度(字节)
 * @param   frame_idx [in]     帧序号(1起, 用于标题行)
 * @param   p_nodes   [in/out] 节点数组(追加写)
 * @param   p_count   [in/out] 当前节点数(进入时为已有数量, 返回时累加)
 * @param   node_cap  [in]     节点数组容量
 * @return  1: 帧解析成功; 0: 失败(失败原因已写入节点)
 */
uint32_t gb32960_parse_frame_nodes(const uint8_t *p_frame, uint32_t frame_len,
                                   uint32_t frame_idx, GB32960_NODE_T *p_nodes,
                                   uint32_t *p_count, uint32_t node_cap);

/**
 * @brief   节点数组转缩进文本报告(帧间空行分隔)
 * @param   p_nodes  [in]  节点数组
 * @param   count    [in]  节点数
 * @param   p_out    [out] 文本输出缓冲
 * @param   out_cap  [in]  输出容量(宽字符)
 * @return  无
 */
void gb32960_nodes_to_text(const GB32960_NODE_T *p_nodes, uint32_t count,
                           wchar_t *p_out, uint32_t out_cap);

/**
 * @brief   命令标识取描述文本(记录摘要等复用)
 * @param   cmd 命令标识
 * @return  描述文本(静态存储)
 */
const wchar_t *gb32960_cmd_desc(uint8_t cmd);

#endif /* GB32960_H */
