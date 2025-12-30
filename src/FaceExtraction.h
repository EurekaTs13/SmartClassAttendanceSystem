#ifndef FACEEXTRACTION_H
#define FACEEXTRACTION_H

#include <QMainWindow>
#include <QTimer>
#include <QString>          // ✅ 必须
#include <QList>            // ✅ 必须

#include <opencv2/opencv.hpp>

#include "StudentInfo.h"    // ✅ 必须（因为 QList<StudentInfo> 是值类型）

QT_BEGIN_NAMESPACE
namespace Ui {
    class FaceExtraction;
}
QT_END_NAMESPACE

class FaceExtraction : public QMainWindow
{
    Q_OBJECT

public:
    explicit FaceExtraction(
        QWidget* parent,
        const QString& courseId,
        const QString& courseName,
        const int& week,
        const QList<StudentInfo>& students
    );

    ~FaceExtraction();

private slots:
    void onCameraToggleClicked();
    void updateFrame();
    void onCollectClicked();
    void onStopClicked();

private:
    void stopCamera();
    void saveCollectedFaces(const QString& studentId);

private:
    Ui::FaceExtraction* ui;

    QString m_courseId;
    QString m_courseName;
    int m_week;
    QList<StudentInfo> m_students;

    // ===== 默认图片 =====
    QPixmap defaultPixmap;

    QTimer* cameraTimer;
    cv::VideoCapture cap;
    cv::CascadeClassifier faceCascade;

    bool cameraOn;
    int captureCount;
    std::vector<cv::Mat> collectedFaces;
};

#endif // FACEEXTRACTION_H
