// ============================================================================
// 文件名称: metaversesync.h
// 功    能: MQTT 通信模块——负责与元宇宙平台的 MQTT Broker 建立连接、
//           发布传感器数据、订阅并接收控制指令，实现设备与平台双向数据同步。
// 依    赖: QtMqtt / QJsonObject
// ============================================================================
#ifndef METAVERSESYNC_H
#define METAVERSESYNC_H

#include <QObject>
#include <QtMqtt/qmqttclient.h>
#include <QJsonObject>

// @brief MQTT 通信类，封装与元宇宙平台的数据同步逻辑。

// 主要职责：
//  - 连接/断开远程 MQTT Broker。
//  - 向 Sensor Topic 发布本机采集的传感器数据。
//  - 向 Control Topic 发送设备控制指令。
//  - 订阅 SimCmd Topic 接收云端下发的模拟控制命令。
//  - 通过 Qt 信号将连接状态、收发数据异步通知到上层 UI / 业务模块。
class MetaverseSync : public QObject
{
    Q_OBJECT

public:
    // @brief 构造函数。
    // @param parent 父对象指针，用于 Qt 对象树生命周期管理。
    explicit MetaverseSync(QObject *parent = nullptr);

    // @brief 析构函数——安全断开 MQTT 连接。
    ~MetaverseSync();

    // @brief 连接到指定的 MQTT Broker。
    // @param host 目标主机地址（IP 或域名）。
    // @param port 目标端口号（MQTT 默认 1883，TLS 默认 8883）。
    void connectToHost(const QString &host, quint16 port);

    // @brief 断开当前 MQTT 连接。
    void disconnectFromHost();

    // @brief 查询当前 MQTT 客户端是否处于已连接状态。
    // @return true 已连接，false 未连接。
    bool isConnected() const;

    // @brief 发布设备控制指令至 Control Topic。
    // @param device 目标设备名称（如 "lamp"、"fan"、"alarm"）。
    // @param state  目标状态（true = 开启，false = 关闭）。
    void sendControlCmd(const QString &device, bool state);

    // @brief 发布传感器采集数据至 Sensor Topic。
    // @param data 包含温度/湿度/光照/烟雾/可燃气体等字段的 JSON 对象。
    void sendSensorData(const QJsonObject &data);

signals:
    // @brief 信号：MQTT Broker 连接成功。
    void connected();

    // @brief 信号：MQTT Broker 连接断开。
    void disconnected();

    // @brief 信号：MQTT 通信过程中发生错误。
    // @param error 可读的错误描述字符串。
    void errorOccurred(const QString &error);

    // @brief 信号：收到云端下发的设备控制指令。
    // @param device 目标设备名称。
    // @param state  期望的设备状态。
    void controlCmdReceived(const QString &device, bool state);

    // @brief 信号：接收到传感器数据（通常来自其他设备或模拟器）。
    // @param data 传感器数据 JSON 对象。
    void sensorDataReceived(const QJsonObject &data);

private slots:
    // @brief 槽：MQTT 连接成功时的内部回调——触发订阅并转发 connected 信号。
    void onConnected();

    // @brief 槽：MQTT 断开连接时的内部回调——转发 disconnected 信号。
    void onDisconnected();

    // @brief 槽：MQTT 错误时的内部回调——格式化错误信息并转发 errorOccurred 信号。
    // @param error QMqttClient 返回的错误码。
    void onError(QMqttClient::ClientError error);

    // @brief 槽：收到 MQTT 消息时的内部回调——解析 JSON 并路由到对应信号。
    // @param message 消息负载（JSON 字节数组）。
    // @param topic   消息所属的 Topic。
    void onMessageReceived(const QByteArray &message, const QMqttTopicName &topic);

private:
    // @brief 订阅所有需要的 Topic（当前仅订阅 TOPIC_SIMCMD）。
    void subscribeAll();

    QMqttClient *m_client; // Qt MQTT 客户端实例。

    // @name 静态 Topic 常量
    // @{
    static const QString TOPIC_CTRL;   // ///< 控制指令发布 Topic。
    static const QString TOPIC_SENSOR; // 传感器数据发布 Topic（Device → AIOTSIM）。
    static const QString TOPIC_SIMCMD; // 模拟指令订阅 Topic（AIOTSIM → Device）。
    // @}
};

#endif // METAVERSESYNC_H