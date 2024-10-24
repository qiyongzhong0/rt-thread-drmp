/*
 * drmp_backend.c
 *
 * Change Logs:
 * Date           Author            Notes
 * 2022-07-10     qiyongzhong       first version
 */

#include <typedef.h>
#include <stdlib.h>
#include <drmp_cprot.h>
#include <drmp_backend.h>

#ifdef DRMP_USING_SERIAL
#include <rtdevice.h>
#endif

#ifdef DRMP_USING_SOCKET
#include <sys/socket.h>
#include <sys/time.h>
#include <string.h>
#include <netdev.h>
#include <netdb.h>
#endif

#define DRMP_SOCKET_BYTE_TMO_MS      50//socket数据间隔超时

typedef struct{
    void (*destory)(void *hinst);
    void (*set_tmo)(void *hinst, int tmo_ms);
    int (*connect)(void *hinst);//return 0-success
    int (*disconn)(void *hinst);//return 0-success
    int (*send)(void *hinst, const char *buf, int size);//return send length
    int (*recv)(void *hinst, char *buf, int bufsize);//return recv length
}drmp_backend_ops_t;

struct drmp_backend{
    void *hinst;
    const drmp_backend_ops_t *ops;
    const char *name;
    drmp_backend_type_t type;
    bool prtraw;
};

static drmp_backend_t * drmp_backend_create_rs485(const char *serial, int baudrate, int parity, int pin, int level)
{
#ifdef DRMP_USING_SERIAL
    static const drmp_backend_ops_t ops = {
        (void *)rs485_destory,
        (void *)rs485_set_recv_tmo,
        (void *)rs485_connect,
        (void *)rs485_disconn,
        (void *)rs485_send,
        (void *)rs485_recv
    };
    drmp_backend_t * backend = calloc(1, sizeof(drmp_backend_t));
    if (backend == NULL)
    {
        return(NULL);
    }
    backend->hinst = rs485_create(serial, baudrate, parity, pin, level);
    if (backend->hinst == NULL)
    {
        free(backend);
        return(NULL);
    }
    backend->ops = &ops;
    return(backend);
#else
    return(NULL);
#endif
}

#ifdef DRMP_USING_SOCKET
typedef struct{
    int  tmo_ms;
    s16  sock;
    u16  port;
    char ip_str[1];
}drmp_backend_tcp_inst_t;

static drmp_backend_tcp_inst_t * drmp_backend_tcp_create(const char *ip_str, int port)
{
    int size = sizeof(drmp_backend_tcp_inst_t) + strlen(ip_str);
    drmp_backend_tcp_inst_t * hinst = (drmp_backend_tcp_inst_t *)calloc(1, size);
    if (hinst == NULL)
    {
        return(NULL);
    }
    
    hinst->tmo_ms = 500;
    hinst->sock = -1;
    hinst->port = port;
    strcpy(hinst->ip_str, ip_str);

    return(hinst);
}

static void drmp_backend_tcp_destory(drmp_backend_tcp_inst_t *hinst)
{
    if (hinst != NULL)
    {
        free(hinst);
    }
}

static void drmp_backend_tcp_set_tmo(drmp_backend_tcp_inst_t *hinst, int tmo_ms)
{
    hinst->tmo_ms = tmo_ms;

    if (hinst->sock >= 0)
    {
        struct timeval tv;
        tv.tv_sec = DRMP_SOCKET_BYTE_TMO_MS/1000;
        tv.tv_usec = (DRMP_SOCKET_BYTE_TMO_MS % 1000) * 1000;
        setsockopt(hinst->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }
}

static int drmp_backend_tcp_connect(drmp_backend_tcp_inst_t *hinst)
{
    struct netdev *netdev = netdev_get_by_family(AF_INET);
    if (netdev == NULL)
    {
        return(-1);
    }
    
    if (netdev_is_link_up(netdev) == 0)
    {
        return(-1);
    }
    
    struct hostent *host = gethostbyname(hinst->ip_str);
    if (host == RT_NULL)
    {
        return(-1);
    }
    
    hinst->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (hinst->sock < 0)
    {
        return(-1);
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(hinst->port);
    server_addr.sin_addr = *((struct in_addr *)host->h_addr);//netdev_ipaddr_addr(hinst->ip_str);
    rt_memset(&(server_addr.sin_zero), 0, sizeof(server_addr.sin_zero));
    if (connect(hinst->sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) != 0)
    {
        return(-1);
    }
    
    //int tmo_ms = hinst->tmo_ms;
    struct timeval tv;
    tv.tv_sec = DRMP_SOCKET_BYTE_TMO_MS/1000;
    tv.tv_usec = (DRMP_SOCKET_BYTE_TMO_MS % 1000) * 1000;
    setsockopt(hinst->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    return(0);
}

static int drmp_backend_tcp_disconn(drmp_backend_tcp_inst_t *hinst)
{
    return(closesocket(hinst->sock));
}

static int drmp_backend_tcp_send(drmp_backend_tcp_inst_t *hinst, const char *buf, int size)
{
    return(send(hinst->sock, (void *)buf, size, 0));
}

static int drmp_backend_tcp_recv(drmp_backend_tcp_inst_t *hinst, char *buf, int bufsize)
{
    int pos = 0;
    u32 told = rt_tick_get();

    while(pos < bufsize)
    {
        int len = recv(hinst->sock, (void *)buf + pos, bufsize - pos, 0);
        if (len > 0)
        {
            pos += len;
            if (pos % 256)
            {
                break;
            }
            continue;
        }
        if (pos)
        {
            break;
        }
        if (rt_tick_get() - told < DRMP_SOCKET_BYTE_TMO_MS/2)//错误或socket关闭，导致立即返回
        {
            return(-1);
        }
        if (rt_tick_get() - told >= hinst->tmo_ms)//超时了
        {
            break;
        }
    }

    return(pos);
}
#endif

static drmp_backend_t * drmp_backend_create_tcp(const char *ip_str, int port)
{
#ifdef DRMP_USING_SOCKET
    static const drmp_backend_ops_t ops = {
        (void *)drmp_backend_tcp_destory,
        (void *)drmp_backend_tcp_set_tmo,
        (void *)drmp_backend_tcp_connect,
        (void *)drmp_backend_tcp_disconn,
        (void *)drmp_backend_tcp_send,
        (void *)drmp_backend_tcp_recv,
    };
    drmp_backend_t * backend = calloc(1, sizeof(drmp_backend_t));
    if (backend == NULL)
    {
        return(NULL);
    }
    backend->hinst = drmp_backend_tcp_create(ip_str, port);
    if (backend->hinst == NULL)
    {
        free(backend);
        return(NULL);
    }
    backend->ops = &ops;
    return(backend);
#else
    return(NULL);
#endif
}

drmp_backend_t * drmp_backend_create(const drmp_backend_param_t *param)
{
    DRMP_ASSERT(param != NULL);
    
    drmp_backend_t *backend = NULL;
    switch (param->type)
    {
    case DRMP_BACKEND_TYPE_SERIAL:
        DRMP_ASSERT(param->serial.serial != NULL);
        backend = drmp_backend_create_rs485(param->serial.serial, param->serial.baudrate, param->serial.parity, -1, 0);
        break;
    case DRMP_BACKEND_TYPE_TCP:
    case DRMP_BACKEND_TYPE_UDP:
        DRMP_ASSERT(param->socket.ip != NULL);
        DRMP_ASSERT(param->socket.port > 0);
        backend = drmp_backend_create_tcp(param->socket.ip, param->socket.port);
        break;
    default:
        break;
    }
    
    if (backend)
    {
        backend->name = param->name;
        backend->type = param->type;
        backend->prtraw = false;
    }

    return(backend);
}

void drmp_backend_destory(drmp_backend_t *backend)
{
    DRMP_ASSERT(backend != NULL);

    if (backend->ops && backend->ops->destory)
    {
        (backend->ops->destory)(backend->hinst);
    }

    backend->ops = NULL;
    
}

void drmp_backend_set_tmo(drmp_backend_t *backend, int tmo_ms)
{
    DRMP_ASSERT(backend != NULL);
    
    if (backend->ops && backend->ops->set_tmo)
    {
        (backend->ops->set_tmo)(backend->hinst, tmo_ms);
    }
}

int  drmp_backend_connect(drmp_backend_t *backend)
{
    DRMP_ASSERT(backend != NULL);
    
    if (backend->ops && backend->ops->connect)
    {
        int rst = (backend->ops->connect)(backend->hinst);
        
        #ifdef DRMP_USING_EVT_CB
        drmp_evt_cb_backend_connect(backend->name, (rst==0));
        #endif

        return(rst);
    }
    return(-1);
}

int  drmp_backend_disconn(drmp_backend_t *backend)
{
    DRMP_ASSERT(backend != NULL);
    
    if (backend->ops && backend->ops->disconn)
    {
        int rst = (backend->ops->disconn)(backend->hinst);
        
        #ifdef DRMP_USING_EVT_CB
        drmp_evt_cb_backend_disconn(backend->name, (rst==0));
        #endif

        return(rst);
    }
    return(-1);
}

int  drmp_backend_send(drmp_backend_t *backend, const char *pbuf, int size)
{
    DRMP_ASSERT(backend != NULL);
    DRMP_ASSERT(pbuf != NULL);
    
    if (backend->ops && backend->ops->send)
    {
        int len = (backend->ops->send)(backend->hinst, pbuf, size);
        
        if (len > 0)
        {
            if (backend->prtraw)
            {
                //drmp_port_print_raw(backend->name, (void *)pbuf, len, true);
            }
            #ifdef DRMP_USING_EVT_CB
            drmp_evt_cb_backend_send(backend->name, (void *)pbuf, len);
            #endif
        }


        return(len);
    }
    return(-1);
}

int  drmp_backend_recv(drmp_backend_t *backend, char *pbuf, int bufsize)
{
    DRMP_ASSERT(backend != NULL);
    DRMP_ASSERT(pbuf != NULL);
    
    if (backend->ops && backend->ops->recv)
    {
        int len = (backend->ops->recv)(backend->hinst, pbuf, bufsize);

        if (len > 0)
        {
            if (backend->prtraw)
            {
                //drmp_port_print_raw(backend->name, (void *)pbuf, len, false);
            }
            #ifdef DRMP_USING_EVT_CB
            drmp_evt_cb_backend_recv(backend->name, (void *)pbuf, len);
            #endif
        }
        
        return(len);
    }
    return(-1);
}

void drmp_backend_set_prtraw(drmp_backend_t *backend, bool is_on)
{
    DRMP_ASSERT(backend != NULL);
    backend->prtraw = is_on;
}

const char * drmp_backend_get_name(drmp_backend_t *backend)
{
    DRMP_ASSERT(backend != NULL);
    return(backend->name);
}

drmp_backend_type_t  drmp_backend_get_type(drmp_backend_t *backend)
{
    DRMP_ASSERT(backend != NULL);
    return(backend->type);
}
