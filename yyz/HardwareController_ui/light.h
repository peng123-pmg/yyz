#ifndef LIGHT_H
#define LIGHT_H

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

class Light {
public:
    Light() {
        fd = open("/sys/bus/iio/devices/iio:device1/in_illuminance_input",
                  O_RDONLY | O_NONBLOCK);
        if (fd < 0) perror("open light");
    }
    ~Light() { if (fd >= 0) close(fd); }

    Light(const Light &) = delete;
    Light &operator=(const Light &) = delete;

    int get_light() {
        if (fd < 0) return -1;
        char buf[32] = {0};
        if (lseek(fd, 0, SEEK_SET) < 0) return -1;
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n <= 0) return -1;
        buf[n] = '\0';
        int val = -1;
        if (sscanf(buf, "%d", &val) != 1) return -1;
        return val;
    }
private:
    int fd = -1;
};

#endif // LIGHT_H
