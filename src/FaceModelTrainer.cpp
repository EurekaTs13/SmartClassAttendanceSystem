#include "FaceModelTrainer.h"

// ===== Qt =====
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QStringList>
#include <QTextStream>
#include <QMap>

// ===== OpenCV =====
#include <opencv2/opencv.hpp>
#include <opencv2/face.hpp>

// ===== STL =====
#include <vector>


bool FaceModelTrainer::train()
{
    QString faceRootPath =
        "D:/myDownload/vs2022/vs2022Project/FaceAttendanceSystem2/x64/Release/face";
    QString modelPath =
        "D:/myDownload/vs2022/vs2022Project/FaceAttendanceSystem2/x64/Release/lbph_model.xml";

    QDir rootDir(faceRootPath);
    if (!rootDir.exists())
        return false;

    std::vector<cv::Mat> images;
    std::vector<int> labels;
    QMap<int, QString> labelMap;

    int label = 0;

    QFileInfoList studentDirs =
        rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

    cv::CascadeClassifier faceCascade;
    faceCascade.load("D:/myDownload/opencv/opencv/build/etc/haarcascades/haarcascade_frontalface_default.xml");

    for (const QFileInfo& dirInfo : studentDirs) {

        QString studentId = dirInfo.fileName();
        QDir studentDir(dirInfo.absoluteFilePath());

        QFileInfoList imgs =
            studentDir.entryInfoList(QStringList() << "*.jpg" << "*.png");

        for (const QFileInfo& imgInfo : imgs) {

            cv::Mat img = cv::imread(imgInfo.absoluteFilePath().toStdString());
            if (img.empty())
                continue;

            cv::Mat gray;
            cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);

            std::vector<cv::Rect> faces;
            faceCascade.detectMultiScale(gray, faces, 1.1, 5);

            if (faces.empty())
                continue;

            cv::Mat faceROI = gray(faces[0]);
            cv::resize(faceROI, faceROI, cv::Size(200, 200));

            images.push_back(faceROI);
            labels.push_back(label);
        }

        labelMap[label] = studentId;
        label++;
    }

    if (images.empty())
        return false;

    auto recognizer = cv::face::LBPHFaceRecognizer::create();
    recognizer->train(images, labels);
    recognizer->save(modelPath.toStdString());

    // ===== ±£¥Ê label ”≥…‰ =====
    QFile mapFile("D:/myDownload/vs2022/vs2022Project/FaceAttendanceSystem2/x64/Release/label_map.txt");
    if (mapFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&mapFile);
        for (auto it = labelMap.begin(); it != labelMap.end(); ++it) {
            out << it.key() << " " << it.value() << "\n";
        }
    }

    return true;
}
