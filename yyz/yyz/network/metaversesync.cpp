// ============================================================================
// 文件名称: metaversesync.cpp
// 功    能: MetaverseSync 类的实现——MQTT 连接管理、消息收发、JSON 解析与信号转发。
// ============================================================================
#include "metaversesync.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

// ---- 静态 Topic 常量定义 ----
// TOPIC_CTRL:   用于向设备下发控制指令。
// TOPIC_SENSOR: 设备向 AIOT 模拟器上报传感器数据。
// TOPIC_SIMCMD: AIOT 模拟器向设备下发模拟命令。
const QString MetaverseSync::TOPIC_CTRL   = QStringLiteral("sub");
const QString MetaverseSync::TOPIC_SENSOR = QStringLiteral("Device2AIOTSIM");
const QString MetaverseSync::TOPIC_SIMCMD = QStringLiteral("AIOTSIM2Device");

// ---- 构造 / 析构 ----

MetaverseSync::MetaverseSync(QObject *parent)
    : QObject(parent)
    , m_client(new QMqttClient(this)) // 创建 QMqttClient 并挂载到当前对象树
{
    // 将 QMqttClient 的底层信号连接到本类的私有槽，实现事件统一处理与二次转发。
    connect(m_client, &QMqttClient::connected,    this, &MetaverseSync::onConnected);
    connect(m_client, &QMqttClient::disconnected, this, &MetaverseSync::onDisconnected);
    connect(m_client, &QMqttClient::errorChanged, this, &MetaverseSync::onError);
    connect(m_client, &QMqttClient::messageReceived, this, &MetaverseSync::onMessageReceived);
}

MetaverseSync::~MetaverseSync()
{
    // 析构前确保断开连接，避免资源泄漏。
    if (m_client->state() == QMqttClient::Connected) {
        m_client->disconnectFromHost();
    }
}

// ---- 连接管理 ----

void MetaverseSync::connectToHost(const QString &host, quint16 port)
{
    m_client->setHostname(host);
    m_client->setPort(port);
    m_client->connectToHost(); // 异步连接，连接成功后触发 onConnected()
}

void MetaverseSync::disconnectFromHost()
{
    m_client->disconnectFromHost(); // 异步断开，完成后触发 onDisconnected()
}

bool MetaverseSync::isConnected() const
{
    return m_client->state() == QMqttClient::Connected;
}

// ---- 消息发布 ----

void MetaverseSync::sendControlCmd(const QString &device, bool state)
{
    // 构造控制指令 JSON，格式示例：{"lamp":true,"id":0}
    QJsonObject cmd;
    cmd[device] = state;
    cmd["id"] = 0; // 固定 id=0 标识为手动控制指令
    QJsonDocument doc(cmd);

    // 发布到 CONTROL Topic
    m_client->publish(TOPIC_CTRL, doc.toJson(QJsonDocument::Compact));
    qDebug() << "[MetaverseSync] 发送控制指令 ->" << TOPIC_CTRL << doc.toJson(QJsonDocument::Compact);
}

void MetaverseSync::sendSensorData(const QJsonObject &data)
{
    // 将传感器数据 JSON 直接序列化后发布到 SENSOR Topic
    QJsonDocument doc(data);
    m_client->publish(TOPIC_SENSOR, doc.toJson(QJsonDocument::Compact));
    qDebug() << "[MetaverseSync] 发送传感器数据 ->" << TOPIC_SENSOR << doc.toJson(QJsonDocument::Compact);
}

// ---- MQTT 事件回调（私有槽） ----

void MetaverseSync::onConnected()
{
    qDebug() << "[MetaverseSync] MQTT 已连接";
    subscribeAll();     // 连接成功后立即订阅所需 Topic
    emit connected();   // 转发信号通知上层
}

void MetaverseSync::onDisconnected()
{
    qDebug() << "[MetaverseSync] MQTT 已断开";
    emit disconnected(); // 转发信号通知上层
}

void MetaverseSync::onError(QMqttClient::ClientError error)
{
    // 将错误码转为可读字符串并通过信号发出
    QString errMsg = QString("MQTT 错误, 错误码: %1").arg(static_cast<int>(error));
    qDebug() << "[MetaverseSync]" << errMsg;
    emit errorOccurred(errMsg);
}

void MetaverseSync::onMessageReceived(const QByteArray &message, const QMqttTopicName &topic)
{
    qDebug() << "[MetaverseSync] 收到消息 topic:" << topic.name() << "payload:" << message;

    // ---- JSON 解析 ----
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(message, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "[MetaverseSync] JSON 解析失败:" << parseError.errorString();
        return;
    }

    QJsonObject obj = doc.object();

    // ---- 路由：SIMCMD Topic → 控制指令 ----
    if (topic.name() == TOPIC_SIMCMD) {
        // 按预设设备列表逐一匹配，找到则发出 controlCmdReceived 信号。
        const QStringList devices = {"lamp", "fan", "alarm"};
        for (const QString &dev : devices) {
            if (obj.contains(dev)) {
                bool state = obj[dev].toBool();
                emit controlCmdReceived(dev, state);
                return;
            }
        }
        qDebug() << "[MetaverseSync] 未识别的 SIMCMD 消息:" << message;
    }

    // ---- 路由：包含传感器字段 → 传感器数据 ----
    // 检测 JSON 中是否包含温度(tem)、湿度(hum)、光照(light)、可燃气体(flamGas)、烟雾(smog) 等字段
    if (obj.contains("tem") || obj.contains("hum") || obj.contains("light") ||
        obj.contains("flamGas") || obj.contains("smog")) {
        emit sensorDataReceived(obj);
    }
}

void MetaverseSync::subscribeAll()
{
    // 订阅 AIOT 模拟器下发的命令 Topic，以便接收远程控制指令。
    m_client->subscribe(TOPIC_SIMCMD);
    qDebug() << "[MetaverseSync] 已订阅:" << TOPIC_SIMCMD;
}