/*
 * drmp.c
 *
 * Change Logs:
 * Date           Author            Notes
 * 2022-07-10     qiyongzhong       first version
 */

#include <drmp.h>
#include <drmp_frame.h>
#include <drmp_cprot.h>

#define DBG_TAG     "drmp"
#define DBG_LVL     DBG_INFO //DBG_LOG //
#include <rtdbg.h>

extern void drmp_vcom_rx_indicate(drmp_vcom_t *vcom, int size);
extern rt_err_t drmp_vcom_open(rt_device_t dev, rt_uint16_t oflag);
extern rt_err_t drmp_vcom_close(rt_device_t dev);
extern rt_size_t drmp_vcom_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size);
extern rt_size_t drmp_vcom_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size);

static void drmp_set_status(drmp_t *drmp, drmp_status_t status)
{
    drmp->status = status;
    if (status == DRMP_STA_CONNECT)//切换到连接状态
    {
        return;
    }
    
    drmp->conn_step = 0;//复位连接步骤
    
    //关闭所有已打开通道
    for (int ch=1; ch<DRMP_VCOM_TOTAL; ch++)
    {
        if ((drmp->ch_flag & (1 << ch)) == 0)//未打开
        {
            continue;
        }
        drmp->ch_flag &= ~(1 << ch);
        if (drmp->ctrl_cb)
        {
            drmp->ctrl_cb(drmp, ch, DRMP_CC_CLOSE, NULL);
        }
    }
}

static int drmp_frm_send(drmp_t *drmp, const drmp_frm_info_t *frm_info)
{
    int len = drmp_frm_make(drmp->buf, sizeof(drmp->buf), frm_info);
    len = drmp_backend_send(drmp->backend, (void *)(drmp->buf), len);
    if (len <= 0)
    {
        drmp_set_status(drmp, DRMP_STA_STARTUP);
    }
    return(len);
}

static int drmp_frm_recv(drmp_t *drmp, u8 *buf, int bufsize)
{
    int len = drmp_backend_recv(drmp->backend, (void *)buf, bufsize);
    if (len < 0)
    {
        drmp_set_status(drmp, DRMP_STA_STARTUP);
    }
    return(len);
}

static void drmp_frm_deal(drmp_t *drmp, const drmp_frm_info_t *frm_info)
{
    int idx = frm_info->ch;
    if (idx >= DRMP_VCOM_TOTAL)
    {
        return;
    }
    if ((drmp->ch_flag & (1 << idx)) == 0)//未打开
    {
        return;
    }
    drmp_vcom_t * vcom = drmp->vcoms[idx];
    if (vcom == NULL)//未加虚拟设备
    {
        return;
    }
    rt_ringbuffer_put(vcom->rx_rb, frm_info->pdata, frm_info->dlen);
    drmp_vcom_rx_indicate(vcom, frm_info->dlen);
}

static void drmp_recv_and_deal(drmp_t *drmp)
{
    int len = drmp_frm_recv(drmp, drmp->buf, sizeof(drmp->buf));
    if (len <= 0)
    {
        return;
    }

    drmp->heart_old = rt_tick_get();
    
    u8 *frm = drmp->buf;
    while(len > 0)
    {
        int flen = 1;
        if (drmp_frm_chk(frm, len))
        {
            drmp_frm_info_t frm_info;
            flen = drmp_frm_parse(frm, &frm_info);
            drmp_frm_deal(drmp, &frm_info);
        }
        frm += flen;
        len -= flen;
    }
}

void drmp_vcoms_send_chk(drmp_t *drmp)
{
    for (int i=0; i<DRMP_VCOM_TOTAL; i++)
    {
        if ((drmp->ch_flag & (1 << i)) == 0)//通道未打开
        {
            continue;
        }
        
        drmp_vcom_t *vcom = drmp->vcoms[i];
        if (vcom == NULL)//虚拟串口未加入
        {
            continue;
        }
        
        if (vcom->status == 0)//虚拟串口未打开
        {
            continue;
        }
        
        int len = rt_ringbuffer_data_len(vcom->tx_rb);
        if (len <= 0)
        {
            continue;
        }
        
        if (vcom->tx_tick_tmo)
        {
            int size = rt_ringbuffer_get_size(vcom->tx_rb);
            if ((len < size/2) && (rt_tick_get() - vcom->tx_tick_old < vcom->tx_tick_tmo))
            {
                continue;
            }
        }
        
        if (len > DRMP_MTU)
        {
            len = DRMP_MTU;
        }
        
        u8 *pdata = drmp->buf + DRMP_FRM_DATA_OFS;
        len = rt_ringbuffer_get(vcom->tx_rb, pdata, len);

        drmp_frm_info_t frm_info;
        frm_info.ch = i;
        frm_info.dlen = len;
        frm_info.pdata = pdata;

        if (drmp_frm_send(drmp, &frm_info) <= 0)
        {
            return;
        }
    }
}

static void drmp_fsm(drmp_t *drmp)
{
    rt_event_recv(drmp->evt, -1, (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR), DRMP_FSM_TMO_MS, NULL);
    
    drmp_recv_and_deal(drmp);
    drmp_cprot_fsm(drmp);
    
    drmp_vcoms_send_chk(drmp);
}

static bool drmp_create_cprot(drmp_t *drmp)
{
    if (drmp_vcom_add(drmp, 0, "cprot", DRMP_CPORT_FIFO_SIZE, DRMP_CPORT_FIFO_SIZE, 0) < 0)
    {
        LOG_E("create cprot fail.");
        return(false);
    }
    drmp->ch_flag |= (1 << 0);
    drmp_vcom_open((rt_device_t)(drmp->vcoms[0]), RT_DEVICE_OFLAG_RDWR);
    return(true);
}

static bool drmp_build_link(drmp_t *drmp)
{
    while(rt_tick_get() - drmp->conn_old < (DRMP_RECONN_TMO_S * RT_TICK_PER_SECOND))
    {
        if (drmp->stop_req)
        {
            return(false);
        }
        rt_thread_mdelay(1000);
    }
    drmp->conn_old = rt_tick_get();

    drmp_backend_disconn(drmp->backend);
    return(drmp_backend_connect(drmp->backend) == 0);
}

static bool drmp_try_test_link(drmp_t *drmp, int tmo_ms, int retry_times)
{
    for (int i=0; i<retry_times; i++)
    {
        drmp_rst_t rst = drmp_test_link(drmp, tmo_ms);
        if (rst == DRMP_RST_OK)
        {
            return(true);
        }
        if (rst == DRMP_RST_DISCONN)
        {
            break;
        }
    }

    return(false);
}

static bool drmp_try_reg_dev(drmp_t *drmp, int tmo_ms, int retry_times)
{
    char reg_str[256];
    memset(reg_str, 0, sizeof(reg_str));
    if (drmp->ctrl_cb)
    {
        drmp->ctrl_cb(drmp, 0, DRMP_CC_GET_REG_MSG, reg_str);
    }
    if (strlen((void *)reg_str) == 0)
    {
        LOG_E("get register message fail.");
        return(false);
    }
    
    for (int i=0; i<retry_times; i++)
    {
        drmp_rst_t rst = drmp_reg_dev(drmp, reg_str, tmo_ms);
        if (rst == DRMP_RST_OK)
        {
            return(true);
        }
        if (rst == DRMP_RST_DISCONN)
        {
            break;
        }
    }

    return(false);
}

static void drmp_startup_deal(drmp_t *drmp)
{
    switch(drmp->conn_step)
    {
    case 0:
        if (drmp_build_link(drmp))
        {
            drmp->conn_step = 1;
            break;
        }
        break;
    case 1:
        if (drmp_try_test_link(drmp, (DRMP_CPORT_RSP_TMO_S * 1000), 5))
        {
            drmp->conn_step = 2;
            break;
        }
        drmp->conn_step = 0;
        break;
    case 2:
        if (drmp_try_reg_dev(drmp, (DRMP_CPORT_RSP_TMO_S * 1000), 3))
        {
            drmp->conn_step = 3;
            break;
        }
        drmp->conn_step = 0;
        break;
    case 3:
        drmp_set_status(drmp, DRMP_STA_CONNECT);
        break;
    default:
        drmp->conn_step = 0;
        break;
    }
}

static void drmp_heart_chk(drmp_t *drmp)
{
    if (rt_tick_get() - drmp->heart_old < (DRMP_HEART_TMO_S * RT_TICK_PER_SECOND))
    {
        return;
    }
    if (drmp_try_test_link(drmp, (DRMP_CPORT_RSP_TMO_S * 1000), 3))
    {
        return;
    }
    drmp_set_status(drmp, DRMP_STA_STARTUP);
}

static void drmp_connect_deal(drmp_t *drmp)
{
    drmp_fsm(drmp);
    drmp_heart_chk(drmp);
}

static void drmp_thread_entry(void *args)
{
    drmp_t *drmp = (drmp_t *)args;
    
    if ( ! drmp_create_cprot(drmp))
    {
        return;
    }
    
    drmp_set_status(drmp, DRMP_STA_STARTUP);
    
    while(1)
    {
        switch (drmp->status)
        {
        case DRMP_STA_STARTUP:
            drmp_startup_deal(drmp);
            break;
        case DRMP_STA_CONNECT:
            drmp_connect_deal(drmp);
            break;
        default:
            drmp_backend_disconn(drmp->backend);
            return;
        }
        if (drmp->stop_req)
        {
            drmp_set_status(drmp, DRMP_STA_STOP);
        }
    }
}

drmp_t * drmp_create(const char *name, drmp_mode_t mode, const drmp_backend_param_t *prm)
{
    RT_ASSERT(name != NULL);
    RT_ASSERT(prm  != NULL);

    drmp_t *drmp = rt_malloc(sizeof(drmp_t));
    if (drmp == NULL)
    {
        LOG_E("drmp create fail.");
        return(NULL);
    }

    drmp->backend = drmp_backend_create(prm);
    if (drmp->backend == NULL)
    {
        free(drmp);
        LOG_E("drmp backend create fail.");
        return(NULL);
    }

    drmp->evt = rt_event_create(name, RT_IPC_FLAG_FIFO);
    if (drmp->backend == NULL)
    {
        drmp_backend_destory(drmp->backend);
        free(drmp);
        LOG_E("drmp backend create fail.");
        return(NULL);
    }
    
    drmp->mode      = mode;
    drmp->name      = name;
    drmp->status    = DRMP_STA_STOP;
    drmp->stop_req  = 0;
    drmp->conn_step = 0;
    drmp->ch_flag   = 0;
    drmp->ctrl_cb   = RT_NULL;
    drmp->thread    = RT_NULL;
    drmp->conn_old  = - ((DRMP_RECONN_TMO_S - DRMP_FIRST_CONN_DLY_S) * RT_TICK_PER_SECOND);

    for (int i=0; i<DRMP_VCOM_TOTAL; i++)
    {
        drmp->vcoms[i] = NULL;
    }

    return(drmp);
}

void drmp_destory(drmp_t *drmp)
{
    RT_ASSERT(drmp != NULL);

    for (int i=0; i<DRMP_VCOM_TOTAL; i++)
    {
        if (drmp->vcoms[i] != NULL)
        {
            drmp_vcom_remove(drmp->vcoms[i]);
        }
    }

    rt_event_delete(drmp->evt);
    drmp_backend_destory(drmp->backend);
    free(drmp);
}

void drmp_set_ctrl_cb(drmp_t *drmp, drmp_ctrl_cb_t ctrl_cb)
{
    DRMP_ASSERT(drmp != RT_NULL);
    drmp->ctrl_cb = ctrl_cb;
}

int drmp_start(drmp_t *drmp)
{
    DRMP_ASSERT(drmp != RT_NULL);

    if (drmp->status != DRMP_STA_STOP)
    {
        return(RT_EOK);
    }

    if (rt_thread_find((void *)drmp->name) != RT_NULL)
    {
        return(RT_EOK);
    }

    rt_thread_t tid = rt_thread_create(drmp->name, drmp_thread_entry, (void*)drmp, DRMP_THREAD_STK_SIZE, DRMP_THREAD_PRIO, 20);
    if (tid == RT_NULL)
    {
        LOG_E("drmp thread (%s) create fail. no memory.", drmp->name);
        return(-RT_ENOMEM);
    }

    LOG_D("drmp thread (%s) create success.", drmp->name);
    drmp->thread = tid;
    rt_thread_startup(tid);

    return(RT_EOK);
}

void drmp_stop(drmp_t *drmp)
{
    DRMP_ASSERT(drmp != NULL);

    if (drmp->status == DRMP_STA_STOP)
    {
        return;
    }
    
    if (rt_thread_find((void *)drmp->name) == NULL)
    {
        return;
    }
    
    drmp->conn_step = 0;
    drmp->stop_req = 1;
    while(drmp->status != DRMP_STA_STOP) rt_thread_mdelay(5);
    drmp->stop_req = 0;
}

drmp_status_t drmp_get_status(drmp_t *drmp)
{
    DRMP_ASSERT(drmp != NULL);
    return(drmp->status);
}

drmp_rst_t drmp_test_link(drmp_t *drmp, int tmo_ms)
{
    drmp_cprot_send_test(drmp);
    drmp->cprot.test_rst = -1;

    u32 tick_tmo = rt_tick_from_millisecond(tmo_ms);
    u32 tick_old = rt_tick_get();
    while(1)
    {
        (rt_thread_self() == drmp->thread) ? drmp_fsm(drmp) : rt_thread_mdelay(DRMP_FSM_TMO_MS);
        if (drmp->cprot.test_rst >= 0)
        {
            return(drmp->cprot.test_rst);
        }
        if (drmp->conn_step == 0)
        {
            return(DRMP_RST_DISCONN);
        }
        if (rt_tick_get() - tick_old > tick_tmo) 
        {
            break;
        }
    }

    return(DRMP_RST_TMO);
}

#ifdef DRMP_USING_SLAVE
drmp_rst_t drmp_reg_dev(drmp_t *drmp, const char *reg_str, int tmo_ms)
{
    drmp_cprot_send_reg(drmp, reg_str);
    drmp->cprot.reg_rst = -1;

    u32 tick_tmo = rt_tick_from_millisecond(tmo_ms);
    u32 tick_old = rt_tick_get();
    while(1)
    {
        (rt_thread_self() == drmp->thread) ? drmp_fsm(drmp) : rt_thread_mdelay(DRMP_FSM_TMO_MS);
        if (drmp->cprot.reg_rst >= 0)
        {
            return(drmp->cprot.reg_rst);
        }
        if (drmp->conn_step == 0)
        {
            return(DRMP_RST_DISCONN);
        }
        if (rt_tick_get() - tick_old > tick_tmo) 
        {
            break;
        }
    }

    return(DRMP_RST_TMO);
}
#endif

#ifdef DRMP_USING_MASTER
drmp_rst_t drmp_startup_ota(drmp_t *drmp, const char *uri, int tmo_ms)
{
    drmp_cprot_send_ota(drmp, uri);
    drmp->cprot.ota_rst = -1;

    u32 tick_tmo = rt_tick_from_millisecond(tmo_ms);
    u32 tick_old = rt_tick_get();
    while(1)
    {
        (rt_thread_self() == drmp->thread) ? drmp_fsm(drmp) : rt_thread_mdelay(DRMP_FSM_TMO_MS);
        if (drmp->cprot.ota_rst >= 0)
        {
            return(drmp->cprot.ota_rst);
        }
        if (drmp->conn_step == 0)
        {
            return(DRMP_RST_DISCONN);
        }
        if (rt_tick_get() - tick_old > tick_tmo)
        {
            break;
        }
    }

    return(DRMP_RST_TMO);
}
#endif

#ifdef DRMP_USING_MASTER
drmp_rst_t drmp_open_ch(drmp_t *drmp, int ch, int tmo_ms)
{
    drmp_cprot_send_open(drmp, ch);
    drmp->cprot.open_rst = -1;

    u32 tick_tmo = rt_tick_from_millisecond(tmo_ms);
    u32 tick_old = rt_tick_get();
    while(1)
    {
        (rt_thread_self() == drmp->thread) ? drmp_fsm(drmp) : rt_thread_mdelay(DRMP_FSM_TMO_MS);
        if (drmp->cprot.open_rst >= 0)
        {
            return(drmp->cprot.open_rst);
        }
        if (drmp->conn_step == 0)
        {
            return(DRMP_RST_DISCONN);
        }
        if (rt_tick_get() - tick_old > tick_tmo) 
        {
            break;
        }
    }

    return(DRMP_RST_TMO);
}

drmp_rst_t drmp_close_ch(drmp_t *drmp, int ch, int tmo_ms)
{
    drmp_cprot_send_close(drmp, ch);
    drmp->cprot.close_rst = -1;

    u32 tick_tmo = rt_tick_from_millisecond(tmo_ms);
    u32 tick_old = rt_tick_get();
    while(1)
    {
        (rt_thread_self() == drmp->thread) ? drmp_fsm(drmp) : rt_thread_mdelay(DRMP_FSM_TMO_MS);
        if (drmp->cprot.close_rst >= 0)
        {
            return(drmp->cprot.close_rst);
        }
        if (drmp->conn_step == 0)
        {
            return(DRMP_RST_DISCONN);
        }
        if (rt_tick_get() - tick_old > tick_tmo) 
        {
            break;
        }
    }

    return(DRMP_RST_TMO);
}

drmp_rst_t drmp_cfg_ch(drmp_t *drmp, int ch, const char *cfg_str, int tmo_ms)
{
    drmp_cprot_send_cfg(drmp, ch, cfg_str);
    drmp->cprot.cfg_rst = -1;

    u32 tick_tmo = rt_tick_from_millisecond(tmo_ms);
    u32 tick_old = rt_tick_get();
    while(1)
    {
        (rt_thread_self() == drmp->thread) ? drmp_fsm(drmp) : rt_thread_mdelay(DRMP_FSM_TMO_MS);
        if (drmp->cprot.cfg_rst >= 0)
        {
            return(drmp->cprot.cfg_rst);
        }
        if (drmp->conn_step == 0)
        {
            return(DRMP_RST_DISCONN);
        }
        if (rt_tick_get() - tick_old > tick_tmo) 
        {
            break;
        }
    }

    return(DRMP_RST_TMO);
}
#endif

