/*
 * drmp_vcoms.h
 *
 * Change Logs:
 * Date           Author        Notes
 * 2022-07-10     qiyongzhong   create v1.0
 */

#ifndef __DRMP_VCOMS_H__
#define __DRMP_VCOMS_H__

typedef struct {
    struct rt_device parent;    //设备
    struct rt_ringbuffer *rx_rb;//接收缓冲
    struct rt_ringbuffer *tx_rb;//发送缓冲
    u32 tx_tick_old;            //发送启动超时时间点
    u32 tx_tick_tmo;            //发送超时时间
    drmp_t *drmp;               //主设备实例
    int idx;                    //通道索引
    int status;                 //打开状态
}drmp_vcom_t;

int drmp_vcom_add(drmp_t *drmp, int ch, const char *vcom_name, int rx_fifo_size, int tx_fifo_size, int tx_tmo_ms);
int drmp_vcom_remove(drmp_vcom_t *vcom);

#endif

