/*
 * drmp_vcoms.c
 *
 * Change Logs:
 * Date           Author        Notes
 * 2020-12-17     qiyongzhong   create v1.0
 */

#include <drmp.h>
#include <rtthread.h>

#define DBG_TAG     "drmp.vcoms"
#define DBG_LVL     DBG_LOG //DBG_INFO //
#include <rtdbg.h>

rt_err_t drmp_vcom_open(rt_device_t dev, rt_uint16_t oflag)
{
    DRMP_ASSERT(dev != NULL);
    
    drmp_vcom_t *vcom = (drmp_vcom_t *)dev;
    if (vcom->status == 0)
    {
        rt_ringbuffer_reset(vcom->rx_rb);
        rt_ringbuffer_reset(vcom->tx_rb);
        vcom->status = 1;
    }
    
    return(RT_EOK);
}

rt_err_t drmp_vcom_close(rt_device_t dev)
{
    DRMP_ASSERT(dev != NULL);

    drmp_vcom_t *vcom = (drmp_vcom_t *)dev;
    vcom->status = 0;
    return(RT_EOK);
}

rt_size_t drmp_vcom_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    DRMP_ASSERT(dev != NULL);
    DRMP_ASSERT(buffer != NULL);

    drmp_vcom_t *vcom = (drmp_vcom_t *)dev;
    if (vcom->status == 0)//设备未打开
    {
        LOG_E("vcom (%s) write fail. vcom is not opened.", dev->parent.name);
        return(0);
    }

    int idx = vcom->idx;
    drmp_t *drmp = vcom->drmp;
    if ((drmp->ch_flag & (1 << idx)) == 0)//通道未打开
    {
        LOG_E("vcom (%s) write fail. channel is not build.", vcom->parent.parent.name);
        return(0);
    }

    pos = 0;
    while(pos < size)
    {
        int space = 0;
        while ((space = rt_ringbuffer_space_len(vcom->tx_rb)) == 0)
        {
            if (rt_thread_self() == drmp->thread)
            {
                extern void drmp_vcoms_send_chk(drmp_t *drmp);
                drmp_vcoms_send_chk(drmp);
                continue;
            }
            rt_thread_delay(1);
            if ((drmp->ch_flag & (1 << idx)) == 0)//通道关闭
            {
                return(pos);
            }
        }

        int slen = size - pos;
        if (slen > space)
        {
            slen = space;
        }
        
        rt_ringbuffer_put(vcom->tx_rb, buffer + pos, slen);
        vcom->tx_tick_old = rt_tick_get();
        rt_event_send(drmp->evt, (1 << idx));
        pos += slen;
    }

    return(size);
}

rt_size_t drmp_vcom_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    DRMP_ASSERT(dev != NULL);
    DRMP_ASSERT(buffer != NULL);
    
    drmp_vcom_t *vcom = (drmp_vcom_t *)dev;
    return(rt_ringbuffer_get(vcom->rx_rb, buffer, size));
}

const struct rt_device_ops drmp_vcom_ops = {
    NULL,
    drmp_vcom_open,
    drmp_vcom_close,
    drmp_vcom_read,
    drmp_vcom_write,
    NULL
};

void drmp_vcom_rx_indicate(drmp_vcom_t *vcom, int size)
{
    //DRMP_ASSERT(vcom != NULL);
    rt_device_t dev = (rt_device_t)vcom;
    if (dev->rx_indicate)
    {
        dev->rx_indicate((rt_device_t)vcom, size);
    }
}

int drmp_vcom_add(drmp_t *drmp, int ch, const char *vcom_name, int rx_fifo_size, int tx_fifo_size, int tx_tmo_ms)
{
    DRMP_ASSERT(drmp != NULL);
    DRMP_ASSERT((ch >= 0) && (ch < DRMP_VCOM_TOTAL));
    DRMP_ASSERT(vcom_name != NULL);
    DRMP_ASSERT(rx_fifo_size > 0);
    DRMP_ASSERT(tx_fifo_size > 0);

    if (ch >= DRMP_VCOM_TOTAL)
    {
        return(-1);
    }
    
    drmp_vcom_t * vcom = drmp->vcoms[ch];
    if (vcom != NULL)
    {
        return(0);
    }
    vcom = (drmp_vcom_t *)rt_malloc(sizeof(drmp_vcom_t));
    if (vcom == NULL)
    {
        return(-5);
    }

    drmp->vcoms[ch] = vcom;
    vcom->rx_rb = rt_ringbuffer_create(rx_fifo_size);
    vcom->tx_rb = rt_ringbuffer_create(tx_fifo_size);
    vcom->tx_tick_tmo = rt_tick_from_millisecond(tx_tmo_ms);
    vcom->drmp = drmp;
    vcom->idx = ch;
    
    vcom->parent.type = RT_Device_Class_Char;
    vcom->parent.rx_indicate = NULL;
    vcom->parent.tx_complete = NULL;

#ifdef RT_USING_DEVICE_OPS
    vcom->parent.ops = &drmp_vcom_ops;
#else
    vcom->parent.init = NULL;
    vcom->parent.open = drmp_vcom_open;
    vcom->parent.close = drmp_vcom_close;
    vcom->parent.read = drmp_vcom_read;
    vcom->parent.write = drmp_vcom_write;
    vcom->parent.control = NULL;
#endif

    rt_device_register((rt_device_t)vcom, vcom_name, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_STREAM | RT_DEVICE_FLAG_DMA_RX);
    
    return(0);
}

int drmp_vcom_remove(drmp_vcom_t *vcom)
{
    if (vcom == NULL)
    {
        return(0);
    }
    
    drmp_t *drmp = vcom->drmp;
    if ((drmp == NULL) || (vcom->idx > 31) || (drmp->vcoms[vcom->idx] != vcom))
    {
        LOG_E("vcom remove error.");
        return(-1);
    }
    drmp->vcoms[vcom->idx] = NULL;
    drmp->ch_flag &= ~(1 << vcom->idx);
    for (int i=0; i<vcom->parent.ref_count; i++)
    {
        rt_device_close((rt_device_t)vcom);
    }
    rt_ringbuffer_destroy(vcom->rx_rb);
    vcom->rx_rb = NULL;
    rt_ringbuffer_destroy(vcom->tx_rb);
    vcom->tx_rb = NULL;
    rt_device_unregister((rt_device_t)vcom);
    rt_free(vcom);

    return(0);
}
