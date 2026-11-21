/**
 * @file    gb_records.h
 * @author  herongshun
 * @date    2026-11-21
 * @brief   GB32960 解析记录管理接口(纯C, 无界面依赖)
 *          成功解析的帧入库(堆拷贝), 上限GB_REC_RECORD_MAX最旧淘汰;
 *          参照 proto_master(PyQt) HistoryManager: 增/查/清/摘要
 */

#ifndef GB_RECORDS_H
#define GB_RECORDS_H

#include <stdint.h>

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define GB_REC_RECORD_MAX   100u   /**< 记录条数上限(超出淘汰最旧) */
#define GB_REC_SUM_WCHAR    96u    /**< 记录摘要宽字符容量 */

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   清空全部记录(释放堆内存)
 * @param   无
 * @return  无
 */
void gb_records_clear(void);

/**
 * @brief   查询当前记录条数
 * @param   无
 * @return  记录条数
 */
uint32_t gb_records_count(void);

/**
 * @brief   添加一条记录(帧字节堆拷贝入库, 满则淘汰最旧)
 * @param   p_frame [in] 帧数据(含帧头与校验码)
 * @param   len     [in] 帧长度(字节)
 * @return  1: 已添加; 0: 失败(参数无效或内存不足)
 */
uint8_t gb_records_add(const uint8_t *p_frame, uint32_t len);

/**
 * @brief   取指定记录的帧数据指针(库内存储, 勿释放/修改)
 * @param   idx   [in]  记录索引(0起)
 * @param   p_len [out] 帧长度(可为NULL)
 * @return  帧数据指针; 越界返回NULL
 */
const uint8_t *gb_records_get(uint32_t idx, uint32_t *p_len);

/**
 * @brief   生成记录摘要文本(列表显示用): "#序号 命令描述 VIN 长度"
 * @param   idx   [in]  记录索引(0起)
 * @param   p_out [out] 摘要输出缓冲
 * @param   cap   [in]  输出容量(宽字符)
 * @return  无
 */
void gb_records_summary(uint32_t idx, wchar_t *p_out, uint32_t cap);

#endif /* GB_RECORDS_H */
