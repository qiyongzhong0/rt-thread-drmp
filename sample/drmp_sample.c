/*
 * drmp_sample.c
 *
 * Change Logs:
 * Date           Author            Notes
 * 2022-07-13     qiyongzhong       first version
 */

#include <drmp.h>

#ifdef DRMP_USING_SAMPLE

#include <rtthread.h>
#include <string.h>
#include <stdio.h>

#define DRMP_TCP_NAME       "drmp_tcp"
#define DRMP_MSTA_IP        "101.37.77.232"//"72c211ee6ee9ca68.natapp.cc"//主站地址/域名
#define DRMP_MSTA_PORT      5188//主站端口
#define DRMP_MAC_STR        "112233445566"
#define DRMP_PRODUCT_STR    "dtu-hv1.0-n32g452re"
#define DRMP_VER_STR        "v1.00-2024.10.25"

static rt_device_t console_dev_old = NULL;

static drmp_rst_t drmp_get_reg_str_cb(char *str)
{
    sprintf(str, "mac:%s,", DRMP_MAC_STR);
    str += strlen(str);

    sprintf(str, "product:%s,", DRMP_PRODUCT_STR);
    str += strlen(str);
    
    sprintf(str, "fw-ver:%s,", DRMP_VER_STR);
    str += strlen(str);

    sprintf(str, "ch1:console");

    return(DRMP_RST_OK);
}

static drmp_rst_t drmp_get_startup_ota_cb(const char *uri)
{
    extern bool http_ota_startup(const char *uri);
    if (http_ota_startup(uri))
    {
        return(DRMP_RST_OK);
    }
    return(DRMP_RST_FAIL);
}

static drmp_rst_t drmp_open_ch_cb(drmp_t *drmp, int ch)
{
    char name[32];
    sprintf(name, "vcom%02d", ch);

    drmp_rst_t rst = DRMP_RST_OK;
    switch(ch)
    {
    case 1:
        if (console_dev_old != NULL)
        {
            break;
        }
        if (drmp_vcom_add(drmp, ch, name, 256, 1024, 5) != 0)
        {
            rst = DRMP_RST_FAIL;
            break;
        }
        console_dev_old = rt_console_set_device(name);
        if (console_dev_old == NULL)
        {
            drmp_vcom_remove(drmp->vcoms[ch]);
            rst = DRMP_RST_FAIL;
            break;
        }
        finsh_set_device(name);
        break;
    default:
        rst = DRMP_RST_CH_NO;
        break;
    }
    
    return(rst);    
}

static drmp_rst_t drmp_close_ch_cb(drmp_t *drmp, int ch)
{
    drmp_rst_t rst = DRMP_RST_OK;
    switch(ch)
    {
    case 1:
        if (console_dev_old == NULL)
        {
            break;
        }
        rt_console_set_device(console_dev_old->parent.name);
        finsh_set_device(console_dev_old->parent.name);
        console_dev_old = NULL;
        drmp_vcom_remove(drmp->vcoms[ch]);
        break;
    default:
        rst = DRMP_RST_CH_NO;
        break;
    }
    
    return(rst);    
}

static drmp_rst_t drmp_tcp_ctrl_cb(drmp_t *drmp, int ch, drmp_ctrl_cmd_t cmd, void *args)
{
    drmp_rst_t rst = DRMP_RST_OK;
    
    switch(cmd)
    {
    case DRMP_CC_GET_REG_MSG://取注册信息
        rst = drmp_get_reg_str_cb(args);
        break;
    case DRMP_CC_REG://注册设备信息
        break;
    case DRMP_CC_OTA://启动OTA升级
        rst = drmp_get_startup_ota_cb(args);
        break;
    case DRMP_CC_OPEN://打开通道
        rst = drmp_open_ch_cb(drmp, ch);
        break;
    case DRMP_CC_CLOSE://关闭通道
        rst = drmp_close_ch_cb(drmp, ch);
        break;
    case DRMP_CC_CFG://配置通道
        rst = DRMP_RST_CFG_NO;
        break;
    default:
        rst = DRMP_RST_FAIL;
        break;
    }

    return(rst);
}

static int drmp_tcp_startup(void)
{
    drmp_backend_param_t backend;
    backend.name = DRMP_TCP_NAME;
    backend.type = DRMP_BACKEND_TYPE_TCP;
    backend.socket.ip = DRMP_MSTA_IP;
    backend.socket.port = DRMP_MSTA_PORT;
    
    drmp_t *drmp = drmp_create(DRMP_TCP_NAME, DRMP_MODE_SLAVE, &backend);
    RT_ASSERT(drmp != NULL);
    
    drmp_set_ctrl_cb(drmp, drmp_tcp_ctrl_cb);
    drmp_start(drmp);

    return(0);
}
INIT_APP_EXPORT(drmp_tcp_startup);

#endif
