/*
 * drmp_frame.c
 *
 * Change Logs:
 * Date           Author            Notes
 * 2022-07-09     qiyongzhong       first version
 */

#include <drmp_frame.h>
#include <drmp_cfg.h>
#include <crc.h>
#include <string.h>

static u16 drmp_hex2_to_u16(const u8 *phex)
{
    u16 rst = phex[0];
    rst <<= 8;
    rst += phex[1];
    return(rst);
}

static void drmp_u16_to_hex2(u8 *phex, u16 val)
{
    phex[0] = (val >> 8);
    phex[1] = val;
}

int  drmp_frm_make(u8 *buf, int bufsize, const drmp_frm_info_t *frm_info)//生成帧，返回生成的帧长度
{
    if (frm_info->dlen > DRMP_MTU)
    {
        return(0);
    }

    u8 *p = buf;

    drmp_u16_to_hex2(p, DRMP_FRM_HEAD);
    p += 2;
    
    u16 len_area = (frm_info->ch << 12) + frm_info->dlen;
    drmp_u16_to_hex2(p, len_area);
    p += 2;
    
    u16 crc = crc16_cal(frm_info->pdata, frm_info->dlen);
    drmp_u16_to_hex2(p, crc);
    p += 2;
    
    if (p != frm_info->pdata)
    {
        memcpy(p, frm_info->pdata, frm_info->dlen);
    }
    p += frm_info->dlen;

    return((int)(p - buf));
}

bool drmp_frm_chk(const u8 *frm, int flen)//检查帧合法性
{
    if (flen < DRMP_FRM_SIZE_MIN)
    {
        return(false);
    }
    
    u16 frm_head = drmp_hex2_to_u16(frm + 0);
    if (frm_head != DRMP_FRM_HEAD)
    {
        return(false);
    }
    
    u16 len_area = drmp_hex2_to_u16(frm + 2);
    int dlen = (len_area & 0xFFF);
    if (flen < (6 + dlen))
    {
        return(false);
    }
    
    u16 frm_crc = drmp_hex2_to_u16(frm + 4);
    u16 cal_crc = crc16_cal((u8 *)frm + 6, dlen);
    if (frm_crc != cal_crc)
    {
        return(false);
    }
    
    return(true);
}

int  drmp_frm_parse(const u8 *frm, drmp_frm_info_t *frm_info)//解析合法帧，返回整桢长度
{
    u16 len_area = drmp_hex2_to_u16(frm + 2);
    int dlen = (len_area & 0xFFF);
    
    frm_info->ch = (len_area >> 12);
    frm_info->dlen = dlen;
    frm_info->pdata = (u8 *)(frm + 6);

    return(dlen + 6);
}

