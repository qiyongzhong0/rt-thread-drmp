/*
 * drmp_cfg.h
 *
 * Change Logs:
 * Date           Author            Notes
 * 2022-07-09     qiyongzhong       first version
 */

#ifndef __DRMP_CFG_H__
#define __DRMP_CFG_H__

#include <rtconfig.h>

//#define DRMP_USING_SERIAL
#define DRMP_USING_SOCKET
//#define DRMP_USING_MASTER
#define DRMP_USING_SLAVE
//#define DRMP_USING_SAMPLE

#ifndef DRMP_VCOM_TOTAL
#define DRMP_VCOM_TOTAL             2       //虚拟接口总数，最少2个
#endif
#define DRMP_MTU                    1500    //最大传输单元
#define DRMP_BUF_SIZE               2048    //协议处理缓冲区
#define DRMP_FSM_TMO_MS             5       //状态机轮询超时
#define DRMP_CPORT_FIFO_SIZE        256     //控制口FIFO尺寸
#define DRMP_CPORT_RSP_TMO_S        20      //控制口响应超时
#define DRMP_RECONN_TMO_S           (5*60)  //重连间隔超时
#define DRMP_HEART_TMO_S            (3*60)  //心跳超时
#define DRMP_FIRST_CONN_DLY_S       (1*60)  //第一次连接延迟

#define DRMP_THREAD_STK_SIZE        2048
#define DRMP_THREAD_PRIO            8

#define DRMP_ASSERT(x)              RT_ASSERT(x)

#endif

