#include "devicecontrol.h"


deviceControl::deviceControl() {

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
    funcTableOn[HW_FAN] = [this](){
        m_fan.fan_start();
    };

    //依次添加即可
    // funcTableOn[摄像头] = [this](){
    //     摄像头对象.摄像头打开函数();
    // };

    /* 绑定硬件的关闭函数到容器中   */
    funcTableOff[HW_FAN] = [this](){
        m_fan.fan_stop();
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
        devStateBuf[devId] = true;
        func();
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
        devStateBuf[devId] = false;
        func();
    }
}