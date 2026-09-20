#ifndef DEVICECONTROL_H
#define DEVICECONTROL_H

#include <QObject>
#include "functional"
#include "Fan.h"
#include "hardware_def.h"

class deviceControl : public QObject
{
    Q_OBJECT
public:
    deviceControl();

    /*  详细功能跳转可查看   */
    void turnOnSensor(int devId);
    void turnOffSensor(int devId);



private:

    /*  硬件的开关状态 */
    bool devStateBuf[10] = {false};


    /*
        std容器：
        作用：存储各个硬件的接口函数
    */
    std::function<void()> funcTableOn[20];
    std::function<void()> funcTableOff[20];

    // 硬件实例化
    Fan m_fan;
};

#endif // DEVICECONTROL_H
