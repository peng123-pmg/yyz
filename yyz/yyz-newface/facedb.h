#ifndef FACEDB_H
#define FACEDB_H

#include <QString>
#include <QList>
#include <QByteArray>
#include <QSqlDatabase>

/* 成员信息：用于管理员界面展示 */
struct MemberInfo {
    int id = -1;             // 成员 ID（同时作为 LBPH 的 label）
    QString name;            // 成员姓名
    int sampleCount = 0;     // 该成员已采集的人脸样本数量
};

/* SQLite 人脸数据库封装：存储成员与人脸样本 */
class FaceDatabase {
public:
    FaceDatabase() = default;
    ~FaceDatabase();

    /* 打开（不存在则创建）数据库 */
    bool open(const QString &dbPath, QString *error = nullptr);
    void close();

    /* 成员列表（含样本数） */
    QList<MemberInfo> members() const;

    /* 新增成员；成功时可通过 newId 拿到自增 ID */
    bool addMember(const QString &name, int *newId = nullptr, QString *error = nullptr);
    bool removeMember(int memberId, QString *error = nullptr);
    bool memberExists(const QString &name) const;
    QString memberName(int memberId) const;

    /* 保存一张人脸样本（PNG 字节流） */
    bool addSample(int memberId, const QByteArray &pngBytes, QString *error = nullptr);

    /* 人脸样本记录 */
    struct Sample {
        int memberId = -1;
        QByteArray pngBytes;
    };
    QList<Sample> samples() const;

    /* 事务控制：注册新成员时保证原子性 */
    bool transaction();
    bool commit();
    bool rollback();

private:
    bool createTables(QString *error);

    QSqlDatabase m_db;
};

#endif
