#ifndef VIBRATOR_H
#define VIBRATOR_H

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>

class Vibrator {
public:
    Vibrator() : m_fd(-1), m_effectId(-1), m_on(false) {
        m_fd = open("/dev/input/event1", O_RDWR);
        if (m_fd < 0) perror("open vibrator");
    }

    ~Vibrator() {
        if (m_fd >= 0) {
            off();
            close(m_fd);
        }
    }

    Vibrator(const Vibrator &) = delete;
    Vibrator &operator=(const Vibrator &) = delete;

    // duration_ms：震动时长（毫秒），默认 3000ms
    void on(int duration_ms = 3000) {
        if (m_fd < 0) return;
        if (m_on) return;   // 已在震动，避免重复上传 effect

        // 1) 构造并上传 rumble effect
        struct ff_effect effect;
        memset(&effect, 0, sizeof(effect));
        effect.type = FF_RUMBLE;
        effect.id   = -1;                               // -1 = 新建
        effect.u.rumble.strong_magnitude = 0x8000;      // 强度 0~0xFFFF
        effect.u.rumble.weak_magnitude   = 0x8000;
        effect.replay.length = duration_ms;             // 持续时间
        effect.replay.delay  = 0;

        if (ioctl(m_fd, EVIOCSFF, &effect) < 0) {
            perror("EVIOCSFF");
            return;
        }
        m_effectId = effect.id;

        // 2) 播放 effect
        struct input_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.type  = EV_FF;
        ev.code  = m_effectId;
        ev.value = 1;
        if (write(m_fd, &ev, sizeof(ev)) < 0) perror("play vibrator");

        m_on = true;
    }

    void off() {
        if (m_fd < 0 || m_effectId < 0) return;

        // 1) 停止播放
        struct input_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.type  = EV_FF;
        ev.code  = m_effectId;
        ev.value = 0;
        write(m_fd, &ev, sizeof(ev));

        // 2) 删除 effect，释放内核资源
        ioctl(m_fd, EVIOCRMFF, m_effectId);
        m_effectId = -1;
        m_on = false;
    }

    bool is_on() const { return m_on; }

private:
    int  m_fd;
    int  m_effectId;
    bool m_on;
};

#endif // VIBRATOR_H
