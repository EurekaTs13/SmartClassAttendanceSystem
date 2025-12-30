#ifndef ATTENDANCEWINDOW_H
#define ATTENDANCEWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QList>
#include <QString>
#include <QLabel>
#include <QSet>


#include <opencv2/opencv.hpp>
#include <opencv2/face.hpp>

#include "StudentInfo.h"
#include "FaceExtraction.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class AttendanceWindow;
}
QT_END_NAMESPACE

class AttendanceWindow : public QMainWindow
{
    Q_OBJECT


public:
    explicit AttendanceWindow(
        QWidget* parent,
        const QString& courseId,
        const QString& courseName,
        const int& week,
        const QList<StudentInfo>& students
    );

    ~AttendanceWindow();

private slots:
    // ===== 摄像头相关 =====
    void onCameraToggleClicked();   // 开 / 关 摄像头
    void updateFrame();             // 刷新摄像头画面

    // ===== 学生考勤相关 =====
    void onResetClicked();          // 重置所有考勤状态
    void onExportClicked();         // 导出 CSV
    void onStopAttendanceClicked(); // 结束考勤（关闭窗口或回到主界面）

    void onFaceExtractionClicked();  // 进入人脸采集

private:
    // ===== UI =====
    Ui::AttendanceWindow* ui;

    // ===== 课程信息 =====
    QString courseId;
    QString courseName;
    int week;

    // ===== 学生数据 =====
    QList<StudentInfo> students;

    // ===== 摄像头相关 =====
    bool cameraOn = false;
    cv::VideoCapture cap;
    QTimer* cameraTimer = nullptr;

    // 模型相关
    bool recognizerReady = false;


    // ===== 默认图片 =====
    QPixmap defaultPixmap; // ★ 保存原始 face.png

    QString stableName;        // 当前稳定识别到的人
    int stableCount = 0;       // 连续识别成功的帧数

    const int STABLE_FRAMES = 5;   // 连续 5 帧才算稳定
    const double CONF_THRESHOLD = 70; // LBPH 置信度阈值


    // ===== 内部辅助函数 =====
    void initUiContent();           // 初始化右侧信息面板内容
    void initConnections();         // 统一信号槽绑定
    void updateAttendanceTable();   // 刷新学生表
    void stopCamera();              // 安全关闭摄像头

    void loadFaceDatabase();        // 加载人脸数据库

    void loadLabelMap();

    cv::CascadeClassifier faceCascade;   // 人脸检测器

    cv::Ptr<cv::face::LBPHFaceRecognizer> faceRecognizer;   // 人脸识别模型

    std::map<int, QString> labelToStudentId;  // label->studentId 映射

    // 自动签到防重复
    QSet<QString> autoSignedStudents;

    // 悬浮提示
    QLabel* toastLabel = nullptr;
    QTimer* toastTimer = nullptr;

    bool queryAttendance(
        const QString& courseId,
        const QString& studentId,
        int week,
        int& status
    );

    // 已经提示过的“非本课程学生”
    QSet<QString> warnedNotInCourseStudents;

};

#endif // ATTENDANCEWINDOW_H
