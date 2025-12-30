//#pragma once
//
//#include <QtWidgets/QMainWindow>
//#include "ui_FaceAttendanceSystem.h"
//
//class FaceAttendanceSystem : public QMainWindow
//{
//    Q_OBJECT
//
//public:
//    FaceAttendanceSystem(QWidget *parent = nullptr);
//    ~FaceAttendanceSystem();
//
//private:
//    Ui::FaceAttendanceSystemClass ui;
//};
//

#ifndef FACEATTENDANCESYSTEM_H
#define FACEATTENDANCESYSTEM_H

#include <QtWidgets/QMainWindow>
#include <QString>
#include <QList>
#include "studentinfo.h"

QT_BEGIN_NAMESPACE
namespace Ui { class FaceAttendanceSystemClass; }   // 注意这里
QT_END_NAMESPACE


class FaceAttendanceSystem : public QMainWindow
{
    Q_OBJECT

public:
    FaceAttendanceSystem(QWidget* parent = nullptr);
    ~FaceAttendanceSystem();

private slots:
    void onEnterClicked();

private:
    Ui::FaceAttendanceSystemClass* ui;              // 注意这里

    // ================= CSV 读取 =================
    // courseId : 课程号（如 1001）
    // week     : 周次（如 "week01"）
    // courseName : 输出课程名
    // students   : 输出学生列表（含出勤状态）
    bool loadCourseFromSQLite(
        const QString& courseId,
        int week,
        QString& courseName,
        QList<StudentInfo>& students
    );
};

#endif // FACEATTENDANCESYSTEM_H

