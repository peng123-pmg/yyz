/*===============================================
*   文件名称：camera.h
*   描    述：V4L2 摄像头采集封装接口（Qt 工程使用）
*   运行环境：Linux
================================================*/
#ifndef __CAMERA_H__
#define __CAMERA_H__

#ifdef __cplusplus
extern "C" {
#endif

/* 申请的内核缓冲个数 */
#define REQBUFS_COUNT   4

/* 摄像头输出格式 */
typedef enum {
    CAMERA_FMT_UNKNOWN = 0,
    CAMERA_FMT_MJPEG,       /* JPEG 压缩帧，需软件解码 */
    CAMERA_FMT_YUYV         /* YUYV 4:2:2，可快速转 RGB888 */
} camera_fmt_t;

/*
 * 打开并初始化摄像头。
 * devpath : 设备节点，例如 /dev/video0
 * width   : 输入期望宽度，返回实际协商宽度
 * height  : 输入期望高度，返回实际协商高度
 * fmt     : 输入期望格式，返回实际协商格式
 * 返回值  : >=0 为设备 fd，<0 失败
 */
int camera_init(const char *devpath, unsigned int *width, unsigned int *height,
                camera_fmt_t *fmt);

/* 开始视频流采集 */
int camera_start(int fd);

/*
 * 取出一帧数据（阻塞，直到有帧或超时）。
 * buf   : 输出帧数据指针
 * size  : 输出帧数据长度
 * index : 输出缓冲索引，必须用 camera_eqbuf 归还
 */
int camera_dqbuf(int fd, void **buf, unsigned int *size, unsigned int *index);

/* 归还缓冲 */
int camera_eqbuf(int fd, unsigned int index);

/* 停止视频流采集 */
int camera_stop(int fd);

/* 释放资源并关闭设备 */
int camera_exit(int fd);

#ifdef __cplusplus
}
#endif

#endif
