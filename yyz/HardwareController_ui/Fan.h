#ifndef FAN_H
#define FAN_H

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

class Fan {
public:
    Fan() {
        fd = open("/sys/class/hwmon/hwmon1/pwm1", O_WRONLY);
        if (fd < 0) perror("open fan");
        speed = 0;
    }
    ~Fan() { if (fd >= 0) close(fd); }

    Fan(const Fan &) = delete;
    Fan &operator=(const Fan &) = delete;

    void start() {
        if (fd < 0) return;
        char buf[16] = {0};
        int len = snprintf(buf, sizeof(buf), "%d", speed);
        if (len <= 0) return;
        lseek(fd, 0, SEEK_SET);
        if (write(fd, buf, len) < 0) perror("write fan");
    }

    void stop() {
        if (fd < 0) return;
        speed = 0;
        lseek(fd, 0, SEEK_SET);
        if (write(fd, "0", 1) < 0) perror("write fan");
    }

    // 兼容旧接口
    void fan_start() { start(); }
    void fan_stop()  { stop();  }

    void set_speed(int val) {
        if (val < 0)   val = 0;
        if (val > 255) val = 255;
        speed = val;
    }

    int get_speed() const { return speed; }

private:
    int fd = -1;
    int speed = 0;
};

#endif // FAN_H
