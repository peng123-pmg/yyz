#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

class LedController
{
public:
    LedController()
    {
        fd_led[0] = open("/sys/class/leds/led1/brightness", O_WRONLY);
        if (fd_led[0] < 0) perror("open led1");

        fd_led[1] = open("/sys/class/leds/led2/brightness", O_WRONLY);
        if (fd_led[1] < 0) perror("open led2");

        fd_led[2] = open("/sys/class/leds/led3/brightness", O_WRONLY);
        if (fd_led[2] < 0) perror("open led3");
    }

    ~LedController()
    {
        for (int i = 0; i < 3; i++)
        {
            if (fd_led[i] >= 0)
            {
                close(fd_led[i]);
            }
        }
    }

    // 禁止拷贝，和你原代码风格统一
    LedController(const LedController &) = delete;
    LedController &operator=(const LedController &) = delete;

public:
    /**
     * @brief 打开LED
     * @param num LED编号：1,2,3
     */
    void ledOn(int num)
    {
        if (num <1 || num>3)
            return;
        writeFd(fd_led[num - 1], 255);
    }

    /**
     * @brief 关闭LED
     * @param num LED编号：1,2,3
     */
    void ledOff(int num)
    {
        if (num <1 || num>3)
            return;
        writeFd(fd_led[num - 1], 0);
    }

private:
    void writeFd(int fd, int value)
    {
        if (fd < 0) return;
        char buf[16] = {0};
        int len = snprintf(buf, sizeof(buf), "%d", value);
        if (len <=0) return;
        lseek(fd, 0, SEEK_SET);
        if (write(fd, buf, len) <0)
        {
            perror("write led brightness");
        }
    }

    int fd_led[3] = {-1,-1,-1};
};

#endif // LED_CONTROLLER_H
