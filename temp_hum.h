#ifndef TEMP_HUM_H
#define TEMP_HUM_H

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

class TempHum {
public:
    TempHum() {
        fd_temp_raw    = open("/sys/bus/iio/devices/iio:device0/in_temp_raw",
                              O_RDONLY );
        fd_temp_offset = open("/sys/bus/iio/devices/iio:device0/in_temp_offset",
                              O_RDONLY );
        fd_temp_scale  = open("/sys/bus/iio/devices/iio:device0/in_temp_scale",
                              O_RDONLY );

        fd_hum_raw     = open("/sys/bus/iio/devices/iio:device0/in_humidityrelative_raw",
                              O_RDONLY );
        fd_hum_offset  = open("/sys/bus/iio/devices/iio:device0/in_humidityrelative_offset",
                              O_RDONLY );
        fd_hum_scale   = open("/sys/bus/iio/devices/iio:device0/in_humidityrelative_scale",
                              O_RDONLY );

        if (fd_temp_raw    < 0) perror("open temp raw");
        if (fd_temp_offset < 0) perror("open temp offset");
        if (fd_temp_scale  < 0) perror("open temp scale");
        if (fd_hum_raw     < 0) perror("open hum raw");
        if (fd_hum_offset  < 0) perror("open hum offset");
        if (fd_hum_scale   < 0) perror("open hum scale");
    }

    ~TempHum() {
        if (fd_temp_raw    >= 0) close(fd_temp_raw);
        if (fd_temp_offset >= 0) close(fd_temp_offset);
        if (fd_temp_scale  >= 0) close(fd_temp_scale);
        if (fd_hum_raw     >= 0) close(fd_hum_raw);
        if (fd_hum_offset  >= 0) close(fd_hum_offset);
        if (fd_hum_scale   >= 0) close(fd_hum_scale);
    }

    TempHum(const TempHum &) = delete;
    TempHum &operator=(const TempHum &) = delete;

    double get_temp() {
        int raw = 0, offset = 0;
        float scale = 0.0f;
        if (read_int(fd_temp_raw,    &raw)    != 0) return 0.0;
        if (read_int(fd_temp_offset, &offset) != 0) return 0.0;
        if (read_float(fd_temp_scale, &scale) != 0) return 0.0;
        return (raw + offset) * scale / 1000.0;
    }

    double get_hum() {
        int raw = 0, offset = 0;
        float scale = 0.0f;
        if (read_int(fd_hum_raw,    &raw)    != 0) return 0.0;
        if (read_int(fd_hum_offset, &offset) != 0) return 0.0;
        if (read_float(fd_hum_scale, &scale) != 0) return 0.0;
        return (raw + offset) * scale / 1000.0;
    }

private:
    int read_int(int fd, int *out) {
        if (fd < 0 || out == nullptr) return -1;
        char buf[32] = {0};
        if (lseek(fd, 0, SEEK_SET) < 0) return -1;
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n <= 0) return -1;
        buf[n] = '\0';
        return (sscanf(buf, "%d", out) == 1) ? 0 : -1;
    }
    int read_float(int fd, float *out) {
        if (fd < 0 || out == nullptr) return -1;
        char buf[32] = {0};
        if (lseek(fd, 0, SEEK_SET) < 0) return -1;
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n <= 0) return -1;
        buf[n] = '\0';
        return (sscanf(buf, "%f", out) == 1) ? 0 : -1;
    }

    int fd_temp_raw    = -1;
    int fd_temp_offset = -1;
    int fd_temp_scale  = -1;
    int fd_hum_raw     = -1;
    int fd_hum_offset  = -1;
    int fd_hum_scale   = -1;
};

#endif // TEMP_HUM_H
