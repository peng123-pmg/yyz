#ifndef BEEPER_H
#define BEEPER_H

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

class Beeper {
public:
    Beeper() : m_fd(-1), m_on(false) {
        m_fd = open("/dev/input/event0", O_WRONLY);
        if (m_fd < 0) perror("open beeper");
    }

    ~Beeper() {
        if (m_fd >= 0) {
            off();
            close(m_fd);
        }
    }

    Beeper(const Beeper &) = delete;
    Beeper &operator=(const Beeper &) = delete;

    // frequency：蜂鸣器频率（Hz），常见 1000~4000
    void on(int frequency = 1000) {
        if (m_fd < 0) return;
        struct input_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.type  = EV_SND;
        ev.code  = SND_TONE;
        ev.value = frequency;
        if (write(m_fd, &ev, sizeof(ev)) < 0) perror("write beeper");
        m_on = true;
    }

    void off() {
        if (m_fd < 0 || !m_on) return;
        struct input_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.type  = EV_SND;
        ev.code  = SND_TONE;
        ev.value = 0;
        write(m_fd, &ev, sizeof(ev));
        m_on = false;
    }

    bool is_on() const { return m_on; }

private:
    int  m_fd;
    bool m_on;
};

#endif // BEEPER_H
