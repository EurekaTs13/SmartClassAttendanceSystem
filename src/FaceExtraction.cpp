#include "FaceExtraction.h"
#include "ui_FaceExtraction.h"

#include "AttendanceWindow.h"
#include "FaceAttendanceSystem.h"

#include <QMessageBox>
#include <QInputDialog>
#include <QDir>
#include <QImage>
#include <QPixmap>

#include "StudentInfo.h"
#include "FaceModelTrainer.h"
#include <QList>
#include <QString>


FaceExtraction::FaceExtraction(
    QWidget* parent,
    const QString& courseId,
    const QString& courseName,
    const int& week,
    const QList<StudentInfo>& students)
    : QMainWindow(parent),
    ui(new Ui::FaceExtraction),
    defaultPixmap(":/images/face.png"),
    m_courseId(courseId),
    m_courseName(courseName),
    m_week(week),
    m_students(students),
    cameraTimer(new QTimer(this)),   
    cameraOn(false),
    captureCount(0)
{

    ui->setupUi(this);

    this->showMaximized();

    connect(ui->btnCameraToggle, &QPushButton::clicked,
        this, &FaceExtraction::onCameraToggleClicked);
    connect(ui->btnCollectFace, &QPushButton::clicked,
        this, &FaceExtraction::onCollectClicked);
    connect(ui->btnStop, &QPushButton::clicked,
        this, &FaceExtraction::onStopClicked);
    connect(cameraTimer, &QTimer::timeout,
        this, &FaceExtraction::updateFrame);   // 一旦 cameraTimer->start()， Qt 事件循环就会每隔 30ms 调用一次 updateFrame()

    // ===== 右侧课程信息 =====
    ui->labelCourse->setText("课程号：" + courseId);
    ui->labelWeek->setText("周次：" + QString::number(week));

    // 加载人脸模型（注意路径）
    if (!faceCascade.load("D:/myDownload/opencv/opencv/build/etc/haarcascades/haarcascade_frontalface_default.xml")) {
        QMessageBox::warning(this, "错误", "无法加载人脸检测模型");
    }

    // ===== 摄像头定时器 =====
    cameraTimer->setInterval(100); // ~30 FPS

    // 摄像头区域固定大小
    ui->cameraPanel->setFixedSize(1000, 800);

    ui->labelCamera->setPixmap(
        defaultPixmap.scaled(
            ui->labelCamera->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        )
    );
}

FaceExtraction::~FaceExtraction()
{
    stopCamera();
    delete ui;
}

void FaceExtraction::onCameraToggleClicked()
{
    if (!cameraOn) {
        if (!cap.open(0)) {
            QMessageBox::warning(this, "错误", "无法打开摄像头");
            return;
        }

        cameraOn = true;
        captureCount = 0;
        collectedFaces.clear();

        cameraTimer->start();
        ui->btnCameraToggle->setText("关闭摄像头");
        ui->labelStatus->setText("状态：采集中...");
    }
    else {
        stopCamera();

        // 只有采集满 20 张才允许保存
        if (captureCount >= 20) {
            bool ok;
            QString studentId = QInputDialog::getText(
                this,
                "学号输入",
                "请输入您的学号：",
                QLineEdit::Normal,
                "",
                &ok
            );

            if (ok && !studentId.isEmpty()) {
                saveCollectedFaces(studentId);
                QMessageBox::information(this, "完成", "人脸采集完成并保存成功");
            }
        }
        else {
            // 采集未完成，不保存
            QMessageBox::information(
                this,
                "提示",
                QString("采集未完成（%1 / 20），未保存人脸数据").arg(captureCount)
            );
        }
    }

}

void FaceExtraction::stopCamera()
{
    if (cameraTimer->isActive())
        cameraTimer->stop();

    if (cap.isOpened())
        cap.release();

    cameraOn = false;
    ui->btnCameraToggle->setText("开启摄像头");
    ui->labelCamera->setPixmap(
        defaultPixmap.scaled(
            ui->labelCamera->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        )
    );
    ui->labelStatus->setText("状态：等待识别");
}

void FaceExtraction::updateFrame()
{
    cv::Mat frame;
    cap >> frame;
    if (frame.empty()) return;

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    std::vector<cv::Rect> faces;
    faceCascade.detectMultiScale(gray, faces, 1.2, 5);

    for (const auto& face : faces) {
        cv::rectangle(frame, face, cv::Scalar(0, 255, 0), 2);

        if (captureCount < 20) {   // 采集 20 张
            collectedFaces.push_back(gray(face).clone());
            captureCount++;
        }
    }

    QImage img(frame.data, frame.cols, frame.rows,
        frame.step, QImage::Format_BGR888);
    ui->labelCamera->setPixmap(
        QPixmap::fromImage(img).scaled(
            ui->labelCamera->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        )
    );

    if (captureCount >= 20) {
        onCameraToggleClicked(); // 自动停止
    }
}

void FaceExtraction::saveCollectedFaces(const QString& studentId)
{
    QString basePath =
        "D:/myDownload/vs2022/vs2022Project/"
        "FaceAttendanceSystem2/x64/Release/face/";

    QDir dir;
    QString studentDir = basePath + studentId;
    dir.mkpath(studentDir);

    for (int i = 0; i < collectedFaces.size(); ++i) {
        QString filePath =
            studentDir + QString("/face_%1.jpg").arg(i + 1);
        cv::imwrite(filePath.toStdString(), collectedFaces[i]);
    }
}

void FaceExtraction::onCollectClicked()
{

    stopCamera();

    FaceModelTrainer::train();   // 重新训练并保存模型

    AttendanceWindow* win =
        new AttendanceWindow(nullptr, m_courseId, m_courseName, m_week, m_students);
    win->show();
    this->close();
}

void FaceExtraction::onStopClicked()
{
    stopCamera();

    FaceModelTrainer::train();   // 重新训练并保存模型

    FaceAttendanceSystem* win = new FaceAttendanceSystem();
    win->show();


    this->close();
}
