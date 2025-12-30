#include "FaceAttendanceSystem.h"
#include "ui_FaceAttendanceSystem.h"
#include "AttendanceWindow.h"
#include "DatabaseManager.h"

#include <QMessageBox>
#include <QComboBox>
#include <QLayout>
#include <QBoxLayout>
#include <QFile>
#include <QTextStream>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCoreApplication>
#include <QDebug>



FaceAttendanceSystem::FaceAttendanceSystem(QWidget* parent)
    : QMainWindow(parent),
    ui(new Ui::FaceAttendanceSystemClass)   // 注意这里
{
    ui->setupUi(this);

    this->setStyleSheet(R"(
/* ================= 页面背景 ================= */
QWidget#centralWidget {
    border-image: url(:/images/bg.png) 0 0 0 0 stretch stretch;
}

/* ================= 白色卡片 ================= */
QFrame#cardFrame {
    background-color: rgba(255, 255, 255, 0.95);
    border-radius: 16px;
    padding: 30px;
}

/* ================= 标题 ================= */
QLabel#labelTitle {
    color: #1f3c88;
}

/* ================= 输入框 ================= */
QLineEdit, QComboBox {
    background-color: #ffffff;
    border: 1px solid #d0d4e4;
    border-radius: 6px;
    padding-left: 10px;
    font-size: 14px;
}

QLineEdit:focus, QComboBox:focus {
    border: 1px solid #2a5bd7;
}

/* ================= 按钮 ================= */
QPushButton#btnEnter {
    background-color: #2a5bd7;
    color: white;
    border-radius: 8px;
    font-size: 16px;
    font-weight: bold;
}

QPushButton#btnEnter:hover {
    background-color: #1f4fd8;
}

QPushButton#btnEnter:pressed {
    background-color: #193caa;
}

QFrame#cardFrame {
    background-color: #ffffff;
    border-radius: 14px;
    padding: 30px;
}

)");

    // 替换欢迎界面的课程输入为预设下拉列表（如果 UI 中存在 lineEditCourse）
    //if (ui->lineEditCourse) {
    //    // create combo as child of centralWidget and place at fixed top-left position
    //    QComboBox* comboCourseEntry = new QComboBox(ui->centralWidget);
    //    comboCourseEntry->setObjectName("comboCourseEntry");
    //    comboCourseEntry->addItems({"CS101 - C++ 程序设计", "CS102 - 数据结构", "CS103 - 操作系统", "CS104 - 计算机网络"});
    //    // set width similar to original lineEdit and place near top-left (10,10)
    //    int width = ui->lineEditCourse->width() > 0 ? ui->lineEditCourse->width() : 340;
    //    int height = ui->lineEditCourse->height() > 0 ? ui->lineEditCourse->height() : 36;
    //    comboCourseEntry->setFixedSize(width, height);
    //    comboCourseEntry->move(10, 10);
    //    comboCourseEntry->show();
    //    comboCourseEntry->raise();
    //    // keep original center lineEdit visible so the central blank box remains
    //}

    this->showMaximized();

    connect(ui->btnEnter, &QPushButton::clicked,
        this, &FaceAttendanceSystem::onEnterClicked);
}

FaceAttendanceSystem::~FaceAttendanceSystem()
{
    delete ui;
}

// ================= SQLite 读取 =================
bool FaceAttendanceSystem::loadCourseFromSQLite(
    const QString& courseId,
    int week,
    QString& courseName,
    QList<StudentInfo>& students)
{
    // 1️⃣ 打开数据库（只会创建一次连接）
    QSqlDatabase db = DatabaseManager::db();


    if (!db.open()) {
        qDebug() << "DB open failed:" << db.lastError().text();
        return false;
    }

    // 2️⃣ 查询课程名
    QSqlQuery q1(db);
    q1.prepare("SELECT course_name FROM course WHERE course_id = ?");
    q1.addBindValue(courseId);

    if (!q1.exec() || !q1.next()) {
        qDebug() << "Course not found";
        return false;
    }

    courseName = q1.value(0).toString();

    // 3️⃣ 查询学生 + 出勤状态
    QSqlQuery q2(db);
    q2.prepare(R"(
        SELECT s.student_id, s.student_name, a.status
        FROM attendance a
        JOIN student s ON a.student_id = s.student_id
        WHERE a.course_id = ? AND a.week = ?
    )");

    q2.addBindValue(courseId);
    q2.addBindValue(week);

    if (!q2.exec()) {
        qDebug() << "Attendance query failed:" << q2.lastError().text();
        return false;
    }

    students.clear();

    while (q2.next()) {
        StudentInfo stu;
        stu.studentId = q2.value(0).toString();
        stu.studentName = q2.value(1).toString();
        stu.studentStatus = q2.value(2).toInt() == 1;
        students.append(stu);
    }

    return true;
}


// ================= 进入系统 =================
void FaceAttendanceSystem::onEnterClicked()
{
    QString courseId = ui->lineEditCourse->text().trimmed();
    QString weekText = ui->comboWeek->currentText().trimmed();

    if (courseId.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入课程号！");
        return;
    }

    QString weekNumberStr = weekText;
    weekNumberStr.remove("第");
    weekNumberStr.remove("周");

    bool ok = false;
    int weekNumber = weekNumberStr.toInt(&ok);

    if (!ok || weekNumber <= 0) {
        QMessageBox::warning(this, "提示", "周次格式错误！");
        return;
    }

    // 生成 CSV 中的列名，如 week01
    QString week = QString("week%1").arg(weekNumber, 2, 10, QChar('0'));


    QString courseName;
    QList<StudentInfo> students;

    if (!loadCourseFromSQLite(courseId, weekNumber, courseName, students)) {
        QMessageBox::warning(this, "提示", "未查询到该课程号！?");
        return;
    }

    AttendanceWindow* win =
        new AttendanceWindow(nullptr, courseId, courseName, weekNumber, students);

    win->show();
    this->close();
} 

