#pragma once
#include <QSqlDatabase>

class DatabaseManager {
public:
    static bool init();
    static QSqlDatabase db();
};
