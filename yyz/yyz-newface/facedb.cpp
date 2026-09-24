#include "facedb.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QStringList>

namespace {
const char *kConnectionName = "face_access_connection";
}

FaceDatabase::~FaceDatabase()
{
    close();
}

bool FaceDatabase::open(const QString &dbPath, QString *error)
{
    close();

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                     QString::fromLatin1(kConnectionName));
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) {
        if (error)
            *error = m_db.lastError().text();
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(QString::fromLatin1(kConnectionName));
        return false;
    }

    if (!createTables(error)) {
        close();
        return false;
    }
    return true;
}

void FaceDatabase::close()
{
    if (m_db.isValid())
        m_db.close();

    m_db = QSqlDatabase();
    if (QSqlDatabase::contains(QString::fromLatin1(kConnectionName)))
        QSqlDatabase::removeDatabase(QString::fromLatin1(kConnectionName));
}

bool FaceDatabase::createTables(QString *error)
{
    QSqlQuery q(m_db);

    const QStringList statements = {
        QStringLiteral("PRAGMA foreign_keys = ON"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS members ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT NOT NULL UNIQUE,"
            "created_at TEXT DEFAULT (datetime('now','localtime')))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS face_samples ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "member_id INTEGER NOT NULL,"
            "image BLOB NOT NULL,"
            "created_at TEXT DEFAULT (datetime('now','localtime')),"
            "FOREIGN KEY(member_id) REFERENCES members(id) ON DELETE CASCADE)")
    };

    for (const QString &sql : statements) {
        if (!q.exec(sql)) {
            if (error)
                *error = q.lastError().text();
            return false;
        }
    }
    return true;
}

QList<MemberInfo> FaceDatabase::members() const
{
    QList<MemberInfo> list;
    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral(
            "SELECT m.id, m.name, COUNT(f.id) "
            "FROM members m LEFT JOIN face_samples f ON f.member_id = m.id "
            "GROUP BY m.id, m.name ORDER BY m.id"))) {
        while (q.next()) {
            MemberInfo info;
            info.id = q.value(0).toInt();
            info.name = q.value(1).toString();
            info.sampleCount = q.value(2).toInt();
            list.append(info);
        }
    }
    return list;
}

bool FaceDatabase::addMember(const QString &name, int *newId, QString *error)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO members (name) VALUES (?)"));
    q.addBindValue(name);
    if (!q.exec()) {
        if (error)
            *error = q.lastError().text();
        return false;
    }
    if (newId)
        *newId = q.lastInsertId().toInt();
    return true;
}

bool FaceDatabase::removeMember(int memberId, QString *error)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM members WHERE id = ?"));
    q.addBindValue(memberId);
    if (!q.exec()) {
        if (error)
            *error = q.lastError().text();
        return false;
    }
    return true;
}

bool FaceDatabase::memberExists(const QString &name) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM members WHERE name = ?"));
    q.addBindValue(name);
    if (q.exec() && q.next())
        return q.value(0).toInt() > 0;
    return false;
}

QString FaceDatabase::memberName(int memberId) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT name FROM members WHERE id = ?"));
    q.addBindValue(memberId);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return QString();
}

bool FaceDatabase::addSample(int memberId, const QByteArray &pngBytes, QString *error)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO face_samples (member_id, image) VALUES (?, ?)"));
    q.addBindValue(memberId);
    q.addBindValue(pngBytes);
    if (!q.exec()) {
        if (error)
            *error = q.lastError().text();
        return false;
    }
    return true;
}

QList<FaceDatabase::Sample> FaceDatabase::samples() const
{
    QList<Sample> list;
    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral(
            "SELECT member_id, image FROM face_samples ORDER BY id"))) {
        while (q.next()) {
            Sample s;
            s.memberId = q.value(0).toInt();
            s.pngBytes = q.value(1).toByteArray();
            list.append(s);
        }
    }
    return list;
}

bool FaceDatabase::transaction()
{
    return m_db.transaction();
}

bool FaceDatabase::commit()
{
    return m_db.commit();
}

bool FaceDatabase::rollback()
{
    return m_db.rollback();
}
