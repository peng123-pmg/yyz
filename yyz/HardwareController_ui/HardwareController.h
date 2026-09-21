#ifndef HARDWARECONTROLLER_H
#define HARDWARECONTROLLER_H

#include <QString>
#include <QProcess>
#include <QFile>
#include <QTextStream>
#include <QDebug>

class HardwareController {
public:
    // ============ GPIO 读取 ============
    static int readGPIO(int chip, int line) {
        QProcess process;
        process.start("gpioget",
                      QStringList() << QString::number(chip) << QString::number(line));
        process.waitForFinished(100);
        if (process.exitCode() == 0) {
            return process.readAllStandardOutput().trimmed().toInt();
        }
        return -1;
    }

    // ============ sysfs 文件读写 ============
    static int readFileValue(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return -1;
        QTextStream in(&file);
        int val = 0;
        in >> val;
        file.close();
        return val;
    }

    static void writeFileValue(const QString &path, const QString &val) {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << val;
            file.close();
        }
    }

    // ============ LED 控制 (0~255) ============
    static void setLED(int ledNum, int brightness) {
        if (brightness < 0)   brightness = 0;
        if (brightness > 255) brightness = 255;
        QString path = QString("/sys/class/leds/led%1/brightness").arg(ledNum);
        writeFileValue(path, QString::number(brightness));
    }

    static void setUserLED(int index, int brightness) {
        if (brightness < 0)   brightness = 0;
        if (brightness > 255) brightness = 255;
        QString path = QString("/sys/class/leds/user%1/brightness").arg(index);
        writeFileValue(path, QString::number(brightness));
    }

    static void allLED(int brightness) {
        setLED(1, brightness);
        setLED(2, brightness);
        setLED(3, brightness);
    }

    // ============ 风扇 ============
    static void setFan(int speed) {
        if (speed < 0)   speed = 0;
        if (speed > 255) speed = 255;
        writeFileValue("/sys/class/hwmon/hwmon1/pwm1", QString::number(speed));
    }

    // ============ 蜂鸣器 ============
    static void triggerBeeper() {
        QProcess::startDetached("/home/root/beeper_test");
    }

    static void stopBeeper() {
        QProcess::startDetached("killall", QStringList() << "beeper_test");
    }

    // ============ 震动马达 ============
    static void triggerVibrator() {
        QProcess::startDetached("/home/root/vibrator_test");
    }

    static void stopVibrator() {
        QProcess::startDetached("killall", QStringList() << "vibrator_test");
    }

    // ============ 报警组合 ============
    static void triggerAlarm() {
        triggerBeeper();
        triggerVibrator();
    }

    static void stopAlarm() {
        stopBeeper();
        stopVibrator();
    }
};

#endif // HARDWARECONTROLLER_H
