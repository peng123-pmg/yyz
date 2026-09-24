#ifndef DEVICECONTROL_H
#define DEVICECONTROL_H

#include <QObject>
#include <QTimer>
#include "functional"
#include "Fan.h"
#include "temp_hum.h"
#include "phtotosens.h"
#include "Beeper.h"
#include "hardware_def.h"
#include "Vibrator.h"
#include "leds.h"

class deviceControl : public QObject
{
    Q_OBJECT
public:
    deviceControl();

    /*  详细功能跳转可查看   */
    void turnOnSensor(int devId);
    void turnOffSensor(int devId);

    void turnOnSensor(int devId,int id);
    void turnOffSensor(int devId,int id);

    bool getSenserInfo(int devId,int& val);
    bool getSenserInfo(int devId,double& v1,double& v2);
    bool setSensorVal(int decId,int val);

    // 蜂鸣器滴声
    void beepOnce();

private slots:
    void beepoff();

private:

    /*
        std容器：
        作用：存储各个硬件的接口函数
    */
    std::function<void(int id)> funcTableOn[20];
    std::function<void(int id)> funcTableOff[20];
    std::function<void(double&,double&)> funcTableInfo[10];
    std::function<void(int)> funcTableSet[10];

    // 定时器
    QTimer beeptimer;

    // 硬件实例化
    Fan m_fan;                  // 风扇
    TempHum m_temphum;          // 温湿度传感器
    Photosens m_photosens;              // 光敏传感器
    Beeper  m_beep;             // 蜂鸣器
    Vibrator m_vibrator;        // 震动马达
    LedController m_leds;       // LED控制
};

#endif // DEVICECONTROL_H
