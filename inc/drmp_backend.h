/*
 * drmp_backend.h
 *
 * Change Logs:
 * Date           Author            Notes
 * 2021-12-29     qiyongzhong       first version
 */

#ifndef __DRMP_BACKEND_H__
#define __DRMP_BACKEND_H__

typedef enum{
    DRMP_BACKEND_TYPE_SERIAL = 0,   //串口
    DRMP_BACKEND_TYPE_TCP,          //TCP
    DRMP_BACKEND_TYPE_UDP,          //UDP
}drmp_backend_type_t;//后端类型定义

typedef struct{
    const char *serial;             //串口设备名
    int baudrate;                   //波特率
    int parity;                     //校验位
}drmp_backend_param_serial_t;//串口型后端参数定义

typedef struct{
    const char *ip;                 //IP地址或域名
    int port;                       //端口号
}drmp_backend_param_socket_t;//TCP或UDP型后端参数定义

typedef struct{
    const char *name;               //后端名
    drmp_backend_type_t type;       //后端类型
    union{                          //后端参数联合，须根据类型访问相应参数
        drmp_backend_param_serial_t serial;//串口型后端参数
        drmp_backend_param_socket_t socket;//TCP或UDP型后端参数
    };
}drmp_backend_param_t;//后端参数定义

struct drmp_backend;                //后端数据结构声明
typedef struct drmp_backend drmp_backend_t;//后端数据类型定义

drmp_backend_t * drmp_backend_create(const drmp_backend_param_t *param);
void drmp_backend_destory(drmp_backend_t *backend);
void drmp_backend_set_tmo(drmp_backend_t *backend, int tmo_ms);
int  drmp_backend_connect(drmp_backend_t *backend);
int  drmp_backend_disconn(drmp_backend_t *backend);
int  drmp_backend_send(drmp_backend_t *backend, const char *pbuf, int size);
int  drmp_backend_recv(drmp_backend_t *backend, char *pbuf, int bufsize);
void drmp_backend_set_prtraw(drmp_backend_t *backend, bool is_on);
const char * drmp_backend_get_name(drmp_backend_t *backend);
drmp_backend_type_t drmp_backend_get_type(drmp_backend_t *backend);

#endif

