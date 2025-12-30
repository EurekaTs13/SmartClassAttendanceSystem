#include "FaceAttendanceSystem.h"
#include "DatabaseManager.h"
#include <QtWidgets/QApplication>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <QSqlDatabase>
#include <QDebug>
#include <QMessageBox>


int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // 数据库连接
    if (!DatabaseManager::init()) {
        QMessageBox::critical(nullptr, "错误", "数据库初始化失败");
        return -1;
    }

    FaceAttendanceSystem window;
    window.show();


    qDebug() << "Available SQL drivers:" << QSqlDatabase::drivers();


    return app.exec();   // 必须是 main 的最后一句
}
