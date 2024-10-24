/*
 * drmp_frame.h
 *
 * Change Logs:
 * Date           Author            Notes
 * 2022-07-09     qiyongzhong       first version
 */

#ifndef __DRMP_FRAME_H__
#define __DRMP_FRAME_H__

#include <typedef.h>

#define DRMP_FRM_HEAD       0x524D          //帧头
#define DRMP_FRM_DATA_OFS   6               //数据域偏移
#define DRMP_FRM_SIZE_MIN   (6 + 1)         //最小帧长
#define DRMP_FRM_SIZE_MAX   (6 + DRMP_MTU)  //最大帧长

typedef struct{
    u16 ch;
    u16 dlen;
    u8 *pdata;
}drmp_frm_info_t;

int  drmp_frm_make(u8 *pbuf, int bufsize, const drmp_frm_info_t *frm_info);
bool drmp_frm_chk(const u8 *frm, int flen);
int  drmp_frm_parse(const u8 *frm, drmp_frm_info_t *frm_info);

#endif

