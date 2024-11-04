/*
 * drmp.h
 *
 * Change Logs:
 * Date           Author            Notes
 * 2022-07-10     qiyongzhong       first version
 */

#ifndef __DRMP_H__
#define __DRMP_H__

#include <typedef.h>
#include <drmp_cfg.h>
#include <rtthread.h>
#include <stdlib.h>
#include <string.h>
#include <ipc/ringbuffer.h>

struct drmp;
typedef struct drmp drmp_t;

#include <drmp_backend.h>
#include <drmp_vcoms.h>

typedef enum{
    DRMP_MODE_SLAVE = 0,        //从机模式
    DRMP_MODE_MASTER            //主机模式
}drmp_mode_t;//工作模式定义

typedef enum{
    DRMP_STA_STOP = 0,          //停止状态
    DRMP_STA_STARTUP,           //已启动工作
    DRMP_STA_CONNECT            //已建立连接状态
}drmp_status_t;//工作状态定义

typedef enum{
    DRMP_CC_GET_REG_MSG = 0,    //取注册信息
    DRMP_CC_REG,                //注册设备信息
    DRMP_CC_OTA,                //启动OTA升级
    DRMP_CC_OPEN,               //打开通道
    DRMP_CC_CLOSE,              //关闭通道
    DRMP_CC_CFG                 //配置通道
}drmp_ctrl_cmd_t;//控制回调命令定义
 
typedef enum{
    DRMP_RST_OK = 0,            //成功
    DRMP_RST_FAIL,              //操作失败
    DRMP_RST_CH_NO,             //通道不存在
    DRMP_RST_CFG_NO,            //不支持配置
    DRMP_RST_DISCONN,           //连接断开
    DRMP_RST_TMO = 255,         //超时
}drmp_rst_t;//控制协议结果码定义

//控制协议的命令回调函数定义
typedef drmp_rst_t(*drmp_ctrl_cb_t)(drmp_t *drmp, int ch, drmp_ctrl_cmd_t cmd, void *args);

typedef struct{
    s8 test_rst;
    s8 reg_rst;
    s8 ota_rst;
    s8 open_rst;
    s8 close_rst;
    s8 cfg_rst;
}drmp_cprot_t;

struct drmp{
    u8 mode;
    u8 status;
    u8 stop_req;
    u8 conn_step;
    drmp_cprot_t cprot;
    u32 conn_old;
    u32 heart_old;
    u32 ch_flag;
    rt_event_t evt;
    const char *name;
    rt_thread_t thread;
    drmp_ctrl_cb_t ctrl_cb;
    drmp_backend_t *backend;
    drmp_vcom_t *vcoms[DRMP_VCOM_TOTAL];
    u8 buf[DRMP_BUF_SIZE];
};

drmp_t * drmp_create(const char *name, drmp_mode_t mode, const drmp_backend_param_t *prm);
void drmp_destory(drmp_t *drmp);
void drmp_set_ctrl_cb(drmp_t *drmp, drmp_ctrl_cb_t cb);
int  drmp_start(drmp_t *drmp);
void drmp_stop(drmp_t *drmp);
drmp_status_t drmp_get_status(drmp_t *drmp);

drmp_rst_t drmp_test_link(drmp_t *drmp, int tmo_ms);
#ifdef DRMP_USING_SLAVE
drmp_rst_t drmp_reg_dev(drmp_t *drmp, const char *reg_str, int tmo_ms);
#endif
#ifdef DRMP_USING_MASTER
drmp_rst_t drmp_startup_ota(drmp_t *drmp, const char *uri, int tmo_ms);
drmp_rst_t drmp_open_ch(drmp_t *drmp, int ch, int tmo_ms);
drmp_rst_t drmp_close_ch(drmp_t *drmp, int ch, int tmo_ms);
drmp_rst_t drmp_cfg_ch(drmp_t *drmp, int ch, const char *cfg_str, int tmo_ms);
#endif

#endif

