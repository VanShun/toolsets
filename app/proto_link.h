/**
 * @file    proto_link.h
 * @author  herongshun
 * @date    2026-11-21
 * @brief   协议解析装配层接口(仅合集版编译)
 *          将 gb32960 解析核心注册进 dbgterm 协议解析窗口
 */

#ifndef PROTO_LINK_H
#define PROTO_LINK_H

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   注册合集内可用的协议解析器(主入口初始化时调用一次)
 * @param   无
 * @return  无
 */
void proto_link_init(void);

#endif /* PROTO_LINK_H */
