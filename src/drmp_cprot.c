/*
 * drmp_cprot.h
 *
 * Change Logs:
 * Date           Author            Notes
 * 2022-07-10     qiyongzhong       first version
 */

#include <drmp_cprot.h>

typedef enum{
    DRMP_CMD_TEST = 0,  //测试链路
    DRMP_CMD_TEST_ACK,  //测试链路应答
    DRMP_CMD_REG,       //注册设备
    DRMP_CMD_REG_ACK,   //注册设备应答
    DRMP_CMD_OTA,       //启动OTA
    DRMP_CMD_OTA_ACK,   //启动OTA应答
    DRMP_CMD_OPEN,      //打开通道
    DRMP_CMD_OPEN_ACK,  //打开通道应答
    DRMP_CMD_CLOSE,     //关闭通道
    DRMP_CMD_CLOSE_ACK, //关闭通道应答
    DRMP_CMD_CFG,       //配置通道
    DRMP_CMD_CFG_ACK,   //配置通道应答
}drmp_cprot_cmd_t;

extern rt_size_t drmp_vcom_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size);
extern rt_size_t drmp_vcom_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size);

static int drmp_cprot_send_test_ack(drmp_t *drmp)
{
    u8 datas[32];
    u8 *p = datas;
    *p++ = DRMP_CMD_TEST_ACK;
    *p++ = 0;

    int len = drmp_vcom_write((rt_device_t)(drmp->vcoms[0]), 0, datas, (int)(p - datas));

    return(len);

}
 
#ifdef DRMP_USING_MASTER
static int drmp_cprot_send_reg_ack(drmp_t *drmp, drmp_rst_t rst)
{
    u8 datas[32];
    u8 *p = datas;
    *p++ = DRMP_CMD_REG_ACK;
    *p++ = 0;
    *p++ = rst;
    
    int len = drmp_vcom_write((rt_device_t)(drmp->vcoms[0]), 0, datas, (int)(p - datas));

    return(len);
}
#endif

#ifdef DRMP_USING_SLAVE
static int drmp_cprot_send_ota_ack(drmp_t *drmp, drmp_rst_t rst)
{
    u8 datas[32];
    u8 *p = datas;
    *p++ = DRMP_CMD_OTA_ACK;
    *p++ = 0;
    *p++ = rst;

    int len = drmp_vcom_write((rt_device_t)(drmp->vcoms[0]), 0, datas, (int)(p - datas));

    return(len);
}
#endif

#ifdef DRMP_USING_SLAVE
static int drmp_cprot_send_open_ack(drmp_t *drmp, int ch, drmp_rst_t rst)
{
    u8 datas[32];
    u8 *p = datas;
    *p++ = DRMP_CMD_OPEN_ACK;
    *p++ = ch;
    *p++ = rst;
    
    int len = drmp_vcom_write((rt_device_t)(drmp->vcoms[0]), 0, datas, (int)(p - datas));

    return(len);
}
#endif

#ifdef DRMP_USING_SLAVE
static int drmp_cprot_send_close_ack(drmp_t *drmp, int ch, drmp_rst_t rst)
{
    u8 datas[32];
    u8 *p = datas;
    *p++ = DRMP_CMD_CLOSE_ACK;
    *p++ = ch;
    *p++ = rst;
    
    int len = drmp_vcom_write((rt_device_t)(drmp->vcoms[0]), 0, datas, (int)(p - datas));

    return(len);
}
#endif

#ifdef DRMP_USING_SLAVE
static int drmp_cprot_send_cfg_ack(drmp_t *drmp, int ch, drmp_rst_t rst)
{
    u8 datas[32];
    u8 *p = datas;
    *p++ = DRMP_CMD_CFG_ACK;
    *p++ = ch;
    *p++ = rst;
    
    int len = drmp_vcom_write((rt_device_t)(drmp->vcoms[0]), 0, datas, (int)(p - datas));

    return(len);
}
#endif

static int drmp_cprot_frm_deal(drmp_t *drmp, const u8 *frm, int flen)
{
    if (flen < 2)
    {
        return(flen);
    }

    u8 *p = (u8 *)frm;
    u8 cmd = *p++;
    u8 ch  = *p++;
    switch(cmd)
    {
        case DRMP_CMD_TEST:
            drmp_cprot_send_test_ack(drmp);
            break;
        case DRMP_CMD_TEST_ACK:
            drmp->cprot.test_rst = DRMP_RST_OK;
            break;
        case DRMP_CMD_REG:
        {
            int slen = strlen((void *)p) + 1;
            #ifdef DRMP_USING_MASTER
            drmp_rst_t rst = drmp->ctrl_cb ? drmp->ctrl_cb(drmp, 0, DRMP_CC_REG, p) : DRMP_RST_FAIL;
            drmp_cprot_send_reg_ack(drmp, rst);
            #endif
            p += slen;
            break; 
        }
        case DRMP_CMD_REG_ACK:
            drmp->cprot.reg_rst = *p++;
            break; 
        case DRMP_CMD_OTA:
        {
            int slen = strlen((void *)p) + 1;
            #ifdef DRMP_USING_SLAVE
            drmp_rst_t rst = drmp->ctrl_cb ? drmp->ctrl_cb(drmp, 0, DRMP_CC_OTA, p) : DRMP_RST_FAIL;
            drmp_cprot_send_ota_ack(drmp, rst);
            #endif
            p += slen;
            break;
        }
        case DRMP_CMD_OTA_ACK:
            drmp->cprot.ota_rst = *p++;
            break;
        case DRMP_CMD_OPEN:
        {
            drmp_rst_t rst = DRMP_RST_DISCONN;
            #ifdef DRMP_USING_SLAVE
            if (drmp->status == DRMP_STA_CONNECT)
            {
                rst = drmp->ctrl_cb ? drmp->ctrl_cb(drmp, ch, DRMP_CC_OPEN, NULL) : DRMP_RST_FAIL;
                if (rst == DRMP_RST_OK)
                {
                    drmp->ch_flag |= (1 << ch);
                }
            }
            #endif
            drmp_cprot_send_open_ack(drmp, ch, rst);
            break;
        }
        case DRMP_CMD_OPEN_ACK:
            drmp->cprot.open_rst = *p++;
            break; 
        case DRMP_CMD_CLOSE:
        {
            drmp_rst_t rst = DRMP_RST_DISCONN;
            #ifdef DRMP_USING_SLAVE
            if (drmp->status == DRMP_STA_CONNECT)
            {
                rst = drmp->ctrl_cb ? drmp->ctrl_cb(drmp, ch, DRMP_CC_CLOSE, NULL) : DRMP_RST_FAIL;
                if (rst == DRMP_RST_OK)
                {
                    drmp->ch_flag &= ~(1 << ch);
                }
            }
            #endif
            drmp_cprot_send_close_ack(drmp, ch, rst);
            break; 
        }
        case DRMP_CMD_CLOSE_ACK:
            drmp->cprot.close_rst = *p++;
            break; 
        case DRMP_CMD_CFG:
        {
            int slen = strlen((void *)p) + 1;
            drmp_rst_t rst = DRMP_RST_DISCONN;
            #ifdef DRMP_USING_SLAVE
            if (drmp->status == DRMP_STA_CONNECT)
            {
                rst = drmp->ctrl_cb ? drmp->ctrl_cb(drmp, ch, DRMP_CC_CFG, p) : DRMP_RST_FAIL;
            }
            #endif
            drmp_cprot_send_cfg_ack(drmp, ch, rst);
            p += slen;
            break; 
        }
        case DRMP_CMD_CFG_ACK:
            drmp->cprot.cfg_rst = *p++;
            break; 
        default:
            return(flen);
    }

    return((int)(p - frm));
}

void drmp_cprot_fsm(drmp_t *drmp)
{
    int len = drmp_vcom_read((rt_device_t)(drmp->vcoms[0]), 0, drmp->buf, sizeof(drmp->buf));
    u8 *frm = drmp->buf;
    while(len)
    {
        int flen = drmp_cprot_frm_deal(drmp, frm, len);
        len -= flen;
    }
}

int drmp_cprot_send_test(drmp_t *drmp)
{
    u8 datas[32];
    u8 *p = datas;
    *p++ = DRMP_CMD_TEST;
    *p++ = 0;

    int len = drmp_vcom_write((rt_device_t)(drmp->vcoms[0]), 0, datas, (int)(p - datas));

    return(len);
}

#ifdef DRMP_USING_SLAVE
int drmp_cprot_send_reg(drmp_t *drmp, const char *reg_str)
{
    int slen = strlen(reg_str) + 1;
    u8 *datas = calloc(1, 8 + slen);
    if (datas == NULL)
    {
        return(-1);
    }
    
    u8 *p = datas;
    *p++ = DRMP_CMD_REG;
    *p++ = 0;
    memcpy(p, reg_str, slen);
    p += slen;
    
    int len = drmp_vcom_write((rt_device_t)(drmp->vcoms[0]), 0, datas, (int)(p - datas));

    free(datas);
    
    return(len);
}
#endif

#ifdef DRMP_USING_MASTER
int drmp_cprot_send_ota(drmp_t *drmp, const char *uri)
{
    int slen = strlen(uri) + 1;
    u8 *datas = calloc(1, 8 + slen);
    if (datas == NULL)
    {
        return(-1);
    }

    u8 *p = datas;
    *p++ = DRMP_CMD_OTA;
    *p++ = 0;
    memcpy(p, uri, slen);
    p += slen;

    int len = drmp_vcom_write((rt_device_t)(drmp->vcoms[0]), 0, datas, (int)(p - datas));

    free(datas);

    return(len);
}
#endif

#ifdef DRMP_USING_MASTER
int drmp_cprot_send_open(drmp_t *drmp, int ch)
{
    u8 datas[32];
    u8 *p = datas;
    *p++ = DRMP_CMD_OPEN;
    *p++ = ch;

    int len = drmp_vcom_write((rt_device_t)(drmp->vcoms[0]), 0, datas, (int)(p - datas));

    return(len);
}
#endif

#ifdef DRMP_USING_MASTER
int drmp_cprot_send_close(drmp_t *drmp, int ch)
{
    u8 datas[32];
    u8 *p = datas;
    *p++ = DRMP_CMD_CLOSE;
    *p++ = ch;

    int len = drmp_vcom_write((rt_device_t)(drmp->vcoms[0]), 0, datas, (int)(p - datas));

    return(len);
}
#endif

#ifdef DRMP_USING_MASTER
int drmp_cprot_send_cfg(drmp_t *drmp, int ch, const char *cfg_str)
{
    int slen = strlen(cfg_str) + 1;
    u8 *datas = calloc(1, 8 + slen);
    if (datas == NULL)
    {
        return(-1);
    }
    
    u8 *p = datas;
    *p++ = DRMP_CMD_CFG;
    *p++ = ch;
    memcpy(p, cfg_str, slen);
    p += slen;

    int len = drmp_vcom_write((rt_device_t)(drmp->vcoms[0]), 0, datas, (int)(p - datas));

    free(datas);

    return(len);
}
#endif
