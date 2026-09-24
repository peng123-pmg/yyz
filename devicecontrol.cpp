#include "devicecontrol.h"


deviceControl::deviceControl() {

    connect(&beeptimer,&QTimer::timeout,this,&deviceControl::beepoff);

    for(int i = 0; i < 10; i++)
    {
        funcTableOn[i] = nullptr;
        funcTableOff[i] = nullptr;
    }

    /*  绑定方式：
        1.先在devicecontrol.h 的private：下面实例化硬件
        2.在hardware_def.h文件中，修改宏定义，比如：风扇->0
        3.打开硬件的函数绑定在funcTableOn容器中，反之就绑定在funcTableOff容器中
        4.添加方式：
        funcTableOn[硬件宏] = [this](){
            实例化对象.启动函数();
        };
    */

    /* 绑定硬件的启动函数到容器中   */
    funcTableOn[HW_FAN] = [this](int /*id*/){
        m_fan.fan_start();
    };
    funcTableOn[HW_TEMP_HUB] = [this](int /*id*/){
        m_temphum.get_temp();
        m_temphum.get_hum();
    };
    funcTableOn[HW_PhOTOSENS] = [this](int /*id*/){
        m_photosens.get_Photosens();
    };
    funcTableOn[HW_BEEP] = [this](int /*id*/){
        beepOnce();
    };
    funcTableOn[HW_VIBRATE] = [this](int /*id*/){
        m_vibrator.on();
    };

    funcTableOn[HW_LEDS] = [this](int id){
        m_leds.ledOn(id);
    };

    /* 绑定硬件的关闭函数到容器中   */
    funcTableOff[HW_FAN] = [this](int /*id*/){
        m_fan.fan_stop();
    };
    funcTableOff[HW_VIBRATE] = [this](int /*id*/){
        m_vibrator.off();
    };
    funcTableOff[HW_LEDS] = [this](int id){
        m_leds.ledOff(id);
    };

    /* 绑定硬件的信息参数到容器中   */
    //风扇：只用val1存转速，val2不用
    funcTableInfo[HW_FAN] = [this](double &val1, double &/*val2*/){
        int spd = m_fan.fan_get_speed();
        val1 = static_cast<double>(spd);
    };

    //光敏：只用val1存光照，val2不用
    funcTableInfo[HW_PhOTOSENS] = [this](double &val1, double &/*val2*/){
        int lux = m_photosens.get_Photosens();
        val1 = static_cast<double>(lux);
    };

    //温湿度：val1=温度，val2=湿度，两个都用
    funcTableInfo[HW_TEMP_HUB] = [this](double &temp, double &hum){
        temp = m_temphum.get_temp();
        hum  = m_temphum.get_hum();
    };


    /*   设置硬件的值   */
    funcTableSet[HW_FAN] = [this](int val){
        m_fan.set_speed(val);
    };
}

/*
    @breif: 打开硬件
    @params: 硬件的id ,此项存储在hardware_def.h 文件中
*/
void deviceControl::turnOnSensor(int devId)
{
    if(devId <0 || devId >=10){
        return;
    }

    // 通过函数指针去调用打开传感器
    auto func = funcTableOn[devId];
    if(func)
    {
        func(0);
    }
}

/*
    @breif: 关闭硬件
    @params: 硬件的id ,此项存储在hardware_def.h 文件中
*/

void deviceControl::turnOffSensor(int devId)
{
    if(devId <0 || devId >=10){
        return;
    }

    auto func = funcTableOff[devId];
    if(func)
    {
        func(0);
    }
}

/*
 * @breif: 函数重写，打开对应id设备：led123
 *
*/
void deviceControl::turnOnSensor(int devId,int id)
{
    if(devId <0 || devId >=10){
        return;
    }
    auto func = funcTableOn[devId];
    if(func)
    {
        func(id);
    }

}
void deviceControl::turnOffSensor(int devId,int id)
{
    if(devId <0 || devId >=10){
        return;
    }
    auto func = funcTableOff[devId];
    if(func)
    {
        func(id);
    }
}


/*
    @breif: 获取传感器数据信息
    @params: 传感器ID
*/
bool deviceControl::getSenserInfo(int devId,int &val)
{
    if(devId <0 || devId >=10){
        return false;
    }
    double val_d;
    double dummy;
    auto func = funcTableInfo[devId];
    if(func)
    {
        func(val_d, dummy);   // 传double左值引用，给函数表填充数据
        val = static_cast<int>(val_d); // double转int，赋值输出参数val
        return true;
    }
    return false;
}



bool deviceControl::getSenserInfo(int devId,double &v1,double &v2)
{
    if(devId <0 || devId >=10){
        return false;
    }
    auto func = funcTableInfo[devId];
    if(func)
    {
        func(v1,v2);
        return true;
    }
    return false;
}

/*
    @breif: 设置硬件的参数：风扇转速
    @params: 参数数值
*/
bool deviceControl::setSensorVal(int devId, int val)
{
    if(devId <0 || devId >=10){
        return false;
    }
    auto func = funcTableSet[devId];
    if(func)
    {
        func(val);
        return true;
    }
    return false;
}

void deviceControl::beepoff()
{
    m_beep.off();
}

void deviceControl:: beepOnce()
{
    if(beeptimer.isActive())
    {
        m_beep.off();
        beeptimer.stop();
    }
    m_beep.on();
    beeptimer.start(80);
}
