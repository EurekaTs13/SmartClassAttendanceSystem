#include "DatabaseManager.h"
#include <QCoreApplication>
#include <QSqlError>
#include <QDebug>

static const char* CONN_NAME = "attendance_connection";

bool DatabaseManager::init()
{
    if (QSqlDatabase::contains(CONN_NAME))
        return true;

    QSqlDatabase db =
        QSqlDatabase::addDatabase("QSQLITE", CONN_NAME);

    QString dbPath =
        QCoreApplication::applicationDirPath()
        + "/data/data.db";

    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qDebug() << "DB open failed:" << db.lastError().text();
        return false;
    }

    qDebug() << "DB opened, drivers:" << QSqlDatabase::drivers();
    return true;
}

QSqlDatabase DatabaseManager::db()
{
    return QSqlDatabase::database(CONN_NAME);
}
