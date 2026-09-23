#ifndef HARDWARECONTROLLER_H
#define HARDWARECONTROLLER_H

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <QString>
#include <QProcess>
#include <QStringList>
#include <QFile>
#include <QTextStream>

#include "Beeper.h"
#include "Vibrator.h"

class HardwareController {
public:
    HardwareController() {
        // ---- 打开系统 LED（3 路）----
        fd_led[0] = open("/sys/class/leds/led1/brightness", O_WRONLY);
        if (fd_led[0] < 0) perror("open led1");
        fd_led[1] = open("/sys/class/leds/led2/brightness", O_WRONLY);
        if (fd_led[1] < 0) perror("open led2");
        fd_led[2] = open("/sys/class/leds/led3/brightness", O_WRONLY);
        if (fd_led[2] < 0) perror("open led3");

        // ---- 打开用户 LED（2 路）----
        fd_user[0] = open("/sys/class/leds/user1/brightness", O_WRONLY);
        if (fd_user[0] < 0) perror("open user1");
        fd_user[1] = open("/sys/class/leds/user2/brightness", O_WRONLY);
        if (fd_user[1] < 0) perror("open user2");

        // ---- 打开电位器 ADC（2 路）----
        fd_pot[0] = open("/sys/bus/iio/devices/iio:device3/in_voltage0_raw",
                         O_RDONLY | O_NONBLOCK);
        if (fd_pot[0] < 0) perror("open pot1");
        fd_pot[1] = open("/sys/bus/iio/devices/iio:device3/in_voltage1_raw",
                         O_RDONLY | O_NONBLOCK);
        if (fd_pot[1] < 0) perror("open pot2");
    }

    ~HardwareController() {
        for (int i = 0; i < 3; i++) if (fd_led[i]  >= 0) close(fd_led[i]);
        for (int i = 0; i < 2; i++) if (fd_user[i] >= 0) close(fd_user[i]);
        for (int i = 0; i < 2; i++) if (fd_pot[i]  >= 0) close(fd_pot[i]);
    }

    HardwareController(const HardwareController &) = delete;
    HardwareController &operator=(const HardwareController &) = delete;

    // ============ GPIO 读取 ============
    int readGPIO(int chip, int line) {
        QProcess process;
        process.start("gpioget",
                      QStringList() << QString::number(chip) << QString::number(line));
        process.waitForFinished(100);
        if (process.exitCode() == 0) {
            return process.readAllStandardOutput().trimmed().toInt();
        }
        return -1;
    }

    // ============ 通用文件读取 ============
    int readFileValue(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return -1;
        QTextStream in(&file);
        int val = 0;
        in >> val;
        file.close();
        return val;
    }

    // ============ LED 控制 ============
    void setLED(int ledNum, int brightness) {
        if (ledNum < 1 || ledNum > 3) return;
        if (brightness < 0)   brightness = 0;
        if (brightness > 255) brightness = 255;
        writeFd(fd_led[ledNum - 1], brightness);
    }

    void setUserLED(int index, int brightness) {
        if (index < 1 || index > 2) return;
        if (brightness < 0)   brightness = 0;
        if (brightness > 255) brightness = 255;
        writeFd(fd_user[index - 1], brightness);
    }

    void allLED(int brightness) {
        setLED(1, brightness);
        setLED(2, brightness);
        setLED(3, brightness);
    }

    // ============ 电位器 ADC ============
    int readPot(int index) {
        if (index < 1 || index > 2) return -1;
        int fd = fd_pot[index - 1];
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

    // ============ 蜂鸣器（文件 IO）============
    void triggerBeeper(int freq = 1000) { beeper.on(freq); }
    void stopBeeper()                    { beeper.off();    }

    // ============ 震动马达（文件 IO + ioctl）============
    void triggerVibrator(int ms = 3000)  { vibrator.on(ms); }
    void stopVibrator()                  { vibrator.off();  }

    // ============ 报警组合 ============
    void triggerAlarm() {
        beeper.on(1000);
        vibrator.on(3000);
    }
    void stopAlarm() {
        beeper.off();
        vibrator.off();
    }

private:
    void writeFd(int fd, int value) {
        if (fd < 0) return;
        char buf[16] = {0};
        int len = snprintf(buf, sizeof(buf), "%d", value);
        if (len <= 0) return;
        lseek(fd, 0, SEEK_SET);
        if (write(fd, buf, len) < 0) perror("write");
    }

    int fd_led[3]  = {-1, -1, -1};
    int fd_user[2] = {-1, -1};
    int fd_pot[2]  = {-1, -1};

    Beeper   beeper;     // ← 文件 IO 控制蜂鸣器
    Vibrator vibrator;   // ← 文件 IO + ioctl 控制震动马达
};

#endif // HARDWARECONTROLLER_H
