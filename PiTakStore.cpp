#include "PiTakStore.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>
#include <limits>

PiTakStore::PiTakStore()
    : m_connectionName(QStringLiteral("pitak-%1").arg(reinterpret_cast<quintptr>(this)))
{
}

PiTakStore::~PiTakStore()
{
    if (m_db.isValid())
        m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool PiTakStore::open(const QString &path, QString *error)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error)
            *error = QStringLiteral("Unable to create data directory");
        return false;
    }
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        if (error)
            *error = m_db.lastError().text();
        return false;
    }
    return execSchema(error);
}

bool PiTakStore::execSchema(QString *error)
{
    const QStringList statements = {
        QStringLiteral("PRAGMA foreign_keys=ON"),
        QStringLiteral("PRAGMA journal_mode=WAL"),
        QStringLiteral("PRAGMA synchronous=FULL"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS state (key TEXT PRIMARY KEY, value TEXT NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS inbound_nonces (source TEXT PRIMARY KEY, nonce INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS sessions (id INTEGER PRIMARY KEY AUTOINCREMENT, started_at TEXT NOT NULL, ended_at TEXT, live INTEGER NOT NULL DEFAULT 1)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS observations (id INTEGER PRIMARY KEY AUTOINCREMENT, session_id INTEGER NOT NULL, observed_at TEXT NOT NULL, source TEXT NOT NULL, payload TEXT NOT NULL, FOREIGN KEY(session_id) REFERENCES sessions(id))")
    };
    for (const QString &statement : statements) {
        QSqlQuery query(m_db);
        if (!query.exec(statement)) {
            if (error)
                *error = statement + QStringLiteral(": ") + query.lastError().text();
            return false;
        }
    }

    const QList<QPair<QString, QString>> pragmaChecks = {
        {QStringLiteral("PRAGMA foreign_keys"), QStringLiteral("1")},
        {QStringLiteral("PRAGMA journal_mode"), QStringLiteral("wal")},
        {QStringLiteral("PRAGMA synchronous"), QStringLiteral("2")}
    };
    for (const auto &check : pragmaChecks) {
        QSqlQuery query(m_db);
        if (!query.exec(check.first) || !query.next() ||
            query.value(0).toString().compare(check.second, Qt::CaseInsensitive) != 0) {
            if (error)
                *error = QStringLiteral("SQLite setting not active: ") + check.first;
            return false;
        }
    }
    return true;
}

quint64 PiTakStore::nextOutboundNonce()
{
    if (!m_db.isOpen() || !m_db.transaction())
        return 0;
    QSqlQuery read(m_db);
    read.prepare(QStringLiteral("SELECT value FROM state WHERE key='outbound_nonce'"));
    if (!read.exec()) {
        m_db.rollback();
        return 0;
    }
    quint64 saved = 0;
    if (read.next()) {
        saved = read.value(0).toULongLong();
    } else {
        saved = QRandomGenerator::system()->generate64() &
                static_cast<quint64>(std::numeric_limits<qint64>::max() - 1);
        if (saved == 0)
            saved = 1;
        QSqlQuery seed(m_db);
        seed.prepare(QStringLiteral("INSERT INTO state(key,value) VALUES('outbound_nonce',?)"));
        seed.addBindValue(QString::number(saved));
        if (!seed.exec()) {
            m_db.rollback();
            return 0;
        }
    }
    if (saved >= static_cast<quint64>(std::numeric_limits<qint64>::max())) {
        m_db.rollback();
        return 0;
    }
    const quint64 now = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());
    const quint64 nonce = qMax(saved + 1, now);
    QSqlQuery write(m_db);
    write.prepare(QStringLiteral("INSERT INTO state(key,value) VALUES('outbound_nonce',?) ON CONFLICT(key) DO UPDATE SET value=excluded.value"));
    write.addBindValue(QString::number(nonce));
    if (!write.exec() || !m_db.commit()) {
        m_db.rollback();
        return 0;
    }
    return nonce;
}

bool PiTakStore::acceptInboundNonce(const QString &source, quint64 nonce)
{
    if (!m_db.isOpen() || nonce == 0)
        return false;
    QSqlQuery read(m_db);
    read.prepare(QStringLiteral("SELECT nonce FROM inbound_nonces WHERE source=?"));
    read.addBindValue(source);
    if (!read.exec())
        return false;
    if (read.next() && nonce <= read.value(0).toULongLong())
        return false;
    QSqlQuery write(m_db);
    write.prepare(QStringLiteral("INSERT INTO inbound_nonces(source,nonce) VALUES(?,?) ON CONFLICT(source) DO UPDATE SET nonce=excluded.nonce"));
    write.addBindValue(source);
    write.addBindValue(QString::number(nonce));
    return write.exec();
}

qint64 PiTakStore::startSession()
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("INSERT INTO sessions(started_at,live) VALUES(?,1)"));
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    return query.exec() ? query.lastInsertId().toLongLong() : 0;
}

bool PiTakStore::finishLiveSessions()
{
    if (!m_db.isOpen())
        return false;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE sessions SET live=0, ended_at=? WHERE live=1"));
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    return query.exec();
}

bool PiTakStore::saveObservation(qint64 sessionId, const QString &source, const QString &payload)
{
    if (!sessionId)
        return false;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("INSERT INTO observations(session_id,observed_at,source,payload) VALUES(?,?,?,?)"));
    query.addBindValue(sessionId);
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    query.addBindValue(source);
    query.addBindValue(payload);
    return query.exec();
}

QVariantList PiTakStore::completedSessions() const
{
    QVariantList result;
    QSqlQuery query(m_db);
    query.exec(QStringLiteral("SELECT s.id,s.started_at,COALESCE(s.ended_at,''),COUNT(o.id) FROM sessions s LEFT JOIN observations o ON o.session_id=s.id WHERE s.live=0 GROUP BY s.id ORDER BY s.id DESC"));
    while (query.next()) {
        result.append(QVariantMap{{QStringLiteral("id"), query.value(0)},
                                  {QStringLiteral("startedAt"), query.value(1)},
                                  {QStringLiteral("endedAt"), query.value(2)},
                                  {QStringLiteral("count"), query.value(3)}});
    }
    return result;
}

QVariantList PiTakStore::observations(qint64 sessionId) const
{
    QVariantList result;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT observed_at,source,payload FROM observations WHERE session_id=? ORDER BY id"));
    query.addBindValue(sessionId);
    query.exec();
    while (query.next()) {
        result.append(QVariantMap{{QStringLiteral("observedAt"), query.value(0)},
                                  {QStringLiteral("source"), query.value(1)},
                                  {QStringLiteral("payload"), query.value(2)}});
    }
    return result;
}
