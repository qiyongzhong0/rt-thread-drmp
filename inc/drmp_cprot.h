/*
 * drmp_cprot.h
 *
 * Change Logs:
 * Date           Author            Notes
 * 2022-07-10     qiyongzhong       first version
 */

#ifndef __DRMP_CPROT_H__
#define __DRMP_CPROT_H__

#include <drmp.h>

void drmp_cprot_fsm(drmp_t *drmp);
int drmp_cprot_send_test(drmp_t *drmp);

#ifdef DRMP_USING_SLAVE
int drmp_cprot_send_reg(drmp_t *drmp, const char *reg_str);
#endif

#ifdef DRMP_USING_MASTER
int drmp_cprot_send_ota(drmp_t *drmp, const char *uri);
int drmp_cprot_send_open(drmp_t *drmp, int ch);;
int drmp_cprot_send_close(drmp_t *drmp, int ch);;
int drmp_cprot_send_cfg(drmp_t *drmp, int ch, const char *cfg_str);
#endif

#endif
