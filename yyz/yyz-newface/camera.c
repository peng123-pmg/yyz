/*===============================================
*   文件名称：camera.c
*   描    述：V4L2 摄像头采集封装实现（Linux）
================================================*/
#include "camera.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <linux/videodev2.h>

struct cam_buf {
    void *start;
    size_t length;
};

/* 全局缓冲描述，仅支持单摄像头实例 */
static struct v4l2_requestbuffers reqbufs;
static struct cam_buf bufs[REQBUFS_COUNT];

/* ioctl 封装：被信号中断时自动重试 */
static int xioctl(int fd, unsigned long request, void *arg)
{
    int r;
    do {
        r = ioctl(fd, request, arg);
    } while (r == -1 && errno == EINTR);
    return r;
}

/* 设置采集格式：优先 YUYV（转 RGB 快），失败回退 MJPEG。
 * 只有协商结果为 YUYV 或 MJPEG 时才返回成功，避免把驱动返回的
 * 其它格式误当成 YUYV 处理。 */
static int camera_set_format(int fd, unsigned int *width, unsigned int *height,
                             camera_fmt_t *fmt)
{
    struct v4l2_format format;
    __u32 candidates[2];
    unsigned int req_w = *width;
    unsigned int req_h = *height;
    int i;

    if (*fmt == CAMERA_FMT_MJPEG) {
        candidates[0] = V4L2_PIX_FMT_MJPEG;
        candidates[1] = V4L2_PIX_FMT_YUYV;
    } else {
        candidates[0] = V4L2_PIX_FMT_YUYV;
        candidates[1] = V4L2_PIX_FMT_MJPEG;
    }

    for (i = 0; i < 2; i++) {
        memset(&format, 0, sizeof(format));
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        format.fmt.pix.width = req_w;
        format.fmt.pix.height = req_h;
        format.fmt.pix.pixelformat = candidates[i];
        format.fmt.pix.field = V4L2_FIELD_ANY;

        if (xioctl(fd, VIDIOC_S_FMT, &format) == -1)
            continue;

        if (format.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG) {
            *width = format.fmt.pix.width;
            *height = format.fmt.pix.height;
            *fmt = CAMERA_FMT_MJPEG;
            printf("camera: negotiated MJPEG %ux%u\n", *width, *height);
            return 0;
        }
        if (format.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV) {
            *width = format.fmt.pix.width;
            *height = format.fmt.pix.height;
            *fmt = CAMERA_FMT_YUYV;
            printf("camera: negotiated YUYV %ux%u\n", *width, *height);
            return 0;
        }
        /* 驱动返回了未支持的格式，继续尝试下一个候选格式 */
    }
    return -1;
}

int camera_init(const char *devpath, unsigned int *width, unsigned int *height,
                camera_fmt_t *fmt)
{
    int fd, i;
    struct v4l2_capability capability;
    struct v4l2_buffer vbuf;

    fd = open(devpath, O_RDWR);
    if (fd == -1) {
        perror("camera open");
        return -1;
    }

    if (xioctl(fd, VIDIOC_QUERYCAP, &capability) == -1) {
        perror("VIDIOC_QUERYCAP");
        close(fd);
        return -1;
    }
    if (!(capability.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "camera: device does not support video capture\n");
        close(fd);
        return -1;
    }
    if (!(capability.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "camera: device does not support streaming I/O\n");
        close(fd);
        return -1;
    }

    if (camera_set_format(fd, width, height, fmt) != 0) {
        fprintf(stderr, "camera: cannot set a supported pixel format\n");
        close(fd);
        return -1;
    }

    memset(bufs, 0, sizeof(bufs));

    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.count = REQBUFS_COUNT;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbufs.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd, VIDIOC_REQBUFS, &reqbufs) == -1) {
        perror("VIDIOC_REQBUFS");
        close(fd);
        return -1;
    }
    if (reqbufs.count < 2) {
        fprintf(stderr, "camera: insufficient buffer memory\n");
        close(fd);
        return -1;
    }

    for (i = 0; i < (int)reqbufs.count; i++) {
        memset(&vbuf, 0, sizeof(vbuf));
        vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuf.memory = V4L2_MEMORY_MMAP;
        vbuf.index = i;
        if (xioctl(fd, VIDIOC_QUERYBUF, &vbuf) == -1) {
            perror("VIDIOC_QUERYBUF");
            goto fail_unmap;
        }
        bufs[i].length = vbuf.length;
        bufs[i].start = mmap(NULL, vbuf.length, PROT_READ | PROT_WRITE,
                             MAP_SHARED, fd, vbuf.m.offset);
        if (bufs[i].start == MAP_FAILED) {
            perror("mmap");
            bufs[i].start = NULL;
            goto fail_unmap;
        }
        if (xioctl(fd, VIDIOC_QBUF, &vbuf) == -1) {
            perror("VIDIOC_QBUF");
            goto fail_unmap;
        }
    }

    return fd;

fail_unmap:
    for (i = 0; i < (int)reqbufs.count; i++) {
        if (bufs[i].start != NULL && bufs[i].start != MAP_FAILED)
            munmap(bufs[i].start, bufs[i].length);
        bufs[i].start = NULL;
        bufs[i].length = 0;
    }
    close(fd);
    return -1;
}

int camera_start(int fd)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        perror("VIDIOC_STREAMON");
        return -1;
    }
    printf("camera: capture started\n");
    return 0;
}

int camera_dqbuf(int fd, void **buf, unsigned int *size, unsigned int *index)
{
    fd_set fds;
    struct timeval timeout;
    struct v4l2_buffer vbuf;
    int ret;

    while (1) {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;

        ret = select(fd + 1, &fds, NULL, NULL, &timeout);
        if (ret == -1) {
            if (errno == EINTR)
                continue;
            perror("camera select");
            return -1;
        } else if (ret == 0) {
            fprintf(stderr, "camera: dequeue buffer timeout\n");
            return -1;
        }

        memset(&vbuf, 0, sizeof(vbuf));
        vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuf.memory = V4L2_MEMORY_MMAP;
        if (xioctl(fd, VIDIOC_DQBUF, &vbuf) == -1) {
            if (errno == EAGAIN)
                continue;
            perror("VIDIOC_DQBUF");
            return -1;
        }

        *buf = bufs[vbuf.index].start;
        *size = vbuf.bytesused;
        *index = vbuf.index;
        return 0;
    }
}

int camera_eqbuf(int fd, unsigned int index)
{
    struct v4l2_buffer vbuf;
    memset(&vbuf, 0, sizeof(vbuf));
    vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vbuf.memory = V4L2_MEMORY_MMAP;
    vbuf.index = index;
    if (xioctl(fd, VIDIOC_QBUF, &vbuf) == -1) {
        perror("VIDIOC_QBUF");
        return -1;
    }
    return 0;
}

int camera_stop(int fd)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
        perror("VIDIOC_STREAMOFF");
        return -1;
    }
    printf("camera: capture stopped\n");
    return 0;
}

int camera_exit(int fd)
{
    int i;
    for (i = 0; i < (int)reqbufs.count; i++) {
        if (bufs[i].start != NULL && bufs[i].start != MAP_FAILED)
            munmap(bufs[i].start, bufs[i].length);
        bufs[i].start = NULL;
        bufs[i].length = 0;
    }
    printf("camera: closed\n");
    return close(fd);
}
