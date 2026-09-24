#ifndef PHTOTOSENS_H
#define PHTOTOSENS_H

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

class Photosens {
public:
    Photosens() {
        fd = open("/sys/bus/iio/devices/iio:device1/in_illuminance_input",
                  O_RDONLY);
        if (fd < 0) perror("open Photosens");
    }
    ~Photosens() { if (fd >= 0) close(fd); }

    Photosens(const Photosens &) = delete;
    Photosens &operator=(const Photosens &) = delete;

    int get_Photosens() {
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

#endif // PHTOTOSENS_H
