# drmp软件包

## 1.简介

**drmp** 软件包是用于终端设备远程维护的通信协议栈源码包。终端设备远程维护通信协议，根据终端设备维护需求量身定制，解决终端设备维护困难、工作量大、成本高问题。协议可基于TCP或UDP传输方式工作，推荐使用TCP传输；采用客户端/服务器工作模式，终端设备作为客户端主动连接到设备远程维护主站，并在主站完成注册登陆；主站可对已登陆设备进行远程维护操作，如打开远程控制台、查看设备运行状况、查看实时日志、查看运行参数、实施故障诊断、进行故障修复、启动固件升级等。

### 1.1目录结构

`drmp` 软件包目录结构如下所示：

``` 
drmp
├───doc                                // 说明文档目录
│   |   终端远程维护协议-V1.0.md         // 通信协议说明
│   └───设备远程维护主站使用说明V1.0.pdf  // 主站软件使用说明
├───inc                                // 头文件目录
│   |   drmp.h                         // 协议栈头文件
│   |   drmp_backend.h                 // 通信后端头文件
│   |   drmp_cfg.h                     // 配置头文件
│   |   drmp_cprot.h                   // 控制协议头文件
│   |   drmp_frame.h                   // 帧处理头文件
│   |   drmp_vcoms.h                   // 虚拟串口头文件
│   └───typedef.h                      // 数据类型定义头文件
├───src                                // 源码目录
│   |   drmp.c                         // 协议源文件
│   |   drmp_backend.c                 // 通信后端源文件
│   |   drmp_cprot.c                   // 控制协议源文件
│   |   drmp_frame.c                   // 帧处理源文件
│   └───drmp_vcoms.c                   // 虚拟串口源文件
├───sample                             // 示例代码目录
│   └───drmp_sample.c                  // 协议栈使用示例
├───tools                              // 工具目录 
│   └───设备远程维护主站-V1.23.zip.      // 设备维护主站软件
│   license                            // 软件包许可证
│   readme.md                          // 软件包使用说明
└───SConscript                         // RT-Thread 默认的构建脚本
```

### 1.2许可证

drmp package 遵循 LGPLv2.1 许可，详见 `LICENSE` 文件。

### 1.3依赖

- RT_Thread 4.0
- SAL
- shell

## 2.使用

### 2.1接口函数说明

#### 相关数据类型定义：
```c
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
}drmp_ctrl_cmd_t;//控制协议命令定义
 
typedef enum{
    DRMP_RST_OK = 0,            //成功
    DRMP_RST_FAIL,              //操作失败
    DRMP_RST_CH_NO,             //通道不存在
    DRMP_RST_CFG_NO,            //不支持配置
    DRMP_RST_DISCONN,           //连接断开
    DRMP_RST_TMO = 255,         //超时
}drmp_rst_t;//控制协议结果码定义

//控制协议命令回调函数定义
typedef drmp_rst_t(*drmp_ctrl_cb_t)(drmp_t *drmp, int ch, drmp_ctrl_cmd_t cmd, void *args);

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

```

#### drmp_t * drmp_create(const char *name, drmp_mode_t mode, const drmp_backend_param_t *prm);
- 功能 ：创建设备远程维护协议栈实例
- 参数 ：name--协议栈实例名称
- 参数 ：mode--工作模式
- 参数 ：prm--通信后端配置参数
- 返回 ：成功返回协议栈实例指针，失败返回NULL

#### void drmp_destory(drmp_t *drmp);
- 功能 ：销毁协议栈实例
- 参数 ：drmp--协议栈实例指针
- 返回 ：无

#### void drmp_set_ctrl_cb(drmp_t *drmp, drmp_ctrl_cb_t cb);
- 功能 ：设置控制命令回调函数
- 参数 ：drmp--协议栈实例指针
- 参数 ：cb--回调函数指针
- 返回 ：无

#### int  drmp_start(drmp_t *drmp);
- 功能 ：启动协议栈工作
- 参数 ：drmp--协议栈实例指针
- 返回 ：无

#### void drmp_stop(drmp_t *drmp);
- 功能 ：停止协议栈工作
- 参数 ：drmp--协议栈实例指针
- 返回 ：无

#### drmp_status_t drmp_get_status(drmp_t *drmp);
- 功能 ：获取协议栈工作状态
- 参数 ：drmp--协议栈实例指针
- 返回 ：当前工作状态



### 2.3获取组件

- **方式1：**
通过 *Env配置工具* 或 *RT-Thread studio* 开启软件包，根据需要配置各项参数；配置路径为 *RT-Thread online packages -> peripherals packages -> drmp* 


### 2.4配置参数说明

| 参数宏 | 说明 |
| ---- | ---- |
| DRMP_VCOM_TOTAL       | 使用虚拟串口总数，最少2个
| DRMP_USING_SAMPLE     | 使用示例


## 3. 联系方式

* 维护：qiyongzhong
* 主页：https://github.com/qiyongzhong0/rt-thread-bt_ecb02c
* 主页：https://gitee.com/qiyongzhong0/rt-thread-bt_ecb02c
* 邮箱：917768104@qq.com
