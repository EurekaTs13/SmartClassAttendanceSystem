#include "AttendanceWindow.h"
#include "ui_AttendanceWindow.h"
#include "FaceAttendanceSystem.h"
#include "FaceModelTrainer.h"
#include "DatabaseManager.h"

#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QHeaderView>
#include <QDateTime>
#include <QImage>
#include <QPixmap>
#include <QDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>



static const QString STYLE_PRESENT = R"(
QPushButton {
    background-color: #2ecc71;
    color: white;
    border-radius: 10px;
    padding: 4px 6px;
    font-weight: bold;
}
QPushButton:hover {
    background-color: #27ae60;
}
)";

static const QString STYLE_ABSENT = R"(
QPushButton {
    background-color: #e74c3c;
    color: white;
    border-radius: 10px;
    padding: 4px 6px;
    font-weight: bold;
}
QPushButton:hover {
    background-color: #c0392b;
}
)";


/* =========================================================
 * 构造 / 析构
 * ========================================================= */

AttendanceWindow::AttendanceWindow(
    QWidget* parent,
    const QString& courseId,
    const QString& courseName,
    const int& week,
    const QList<StudentInfo>& students)
    : QMainWindow(parent)
    , ui(new Ui::AttendanceWindow)
    , defaultPixmap(":/images/face.png")   // 只加载，不缩放
    , courseId(courseId)
    , courseName(courseName)
    , week(week)
    , students(students)
{
    ui->setupUi(this);

    // 加载人脸检测模型
    if (!faceCascade.load("D:/myDownload/opencv/opencv/build/etc/haarcascades/haarcascade_frontalface_default.xml")) {
        QMessageBox::critical(this, "错误", "无法加载人脸检测模型");
    }

    // ① 创建识别器
    faceRecognizer = cv::face::LBPHFaceRecognizer::create();

    QString modelPath =
        "D:/myDownload/vs2022/vs2022Project/FaceAttendanceSystem2/x64/Release/lbph_model.xml";

    // ② 判断模型是否存在
    if (QFile::exists(modelPath)) {
        try {
            faceRecognizer->read(modelPath.toStdString());
            loadLabelMap();


            recognizerReady = true;
        }
        catch (...) {
            recognizerReady = false;
        }
    }
    else {
        recognizerReady = false;
    }

    
    this->showMaximized();
    

    initUiContent();
    initConnections();
    updateAttendanceTable();

    ui->labelCamera->setPixmap(
        defaultPixmap.scaled(
            ui->labelCamera->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        )
    );

    // 初始化签到成功悬浮提示

    toastLabel = new QLabel(this);
    toastLabel->setStyleSheet(R"(
    QLabel {
        background-color: rgba(46, 204, 113, 255);  /* 绿底 */
        color: white;                               /* 白字 */
        padding: 10px 20px;
        border-radius: 8px;
        font-size: 14px;
    }
)");
    toastLabel->setAlignment(Qt::AlignCenter);
    toastLabel->hide();

    toastTimer = new QTimer(this);
    toastTimer->setSingleShot(true);
    connect(toastTimer, &QTimer::timeout, toastLabel, &QLabel::hide);

}

AttendanceWindow::~AttendanceWindow()
{
    stopCamera();
    delete ui;
}

/* =========================================================
 * UI 初始化
 * ========================================================= */

void AttendanceWindow::initUiContent()
{
    // ===== 右侧课程信息 =====
    ui->labelCourse->setText("课程号：" + courseId);
    ui->labelWeek->setText("周次：" + QString::number(week));
    ui->labelStatus->setText("状态：等待识别");

    // ===== 学生表格初始化 =====
    ui->studentTable->setColumnCount(3);

    ui->studentTable->setHorizontalHeaderLabels(
        { "学号", "姓名", "状态" }
    );

    ui->studentTable->horizontalHeader()
        ->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->studentTable->horizontalHeader()
        ->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->studentTable->horizontalHeader()
        ->setSectionResizeMode(2, QHeaderView::Fixed);

    ui->studentTable->setColumnWidth(2, 80);

    ui->studentTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->studentTable->setSelectionMode(QAbstractItemView::NoSelection);
    ui->studentTable->setFocusPolicy(Qt::NoFocus);


    // ===== 摄像头定时器 =====
    cameraTimer = new QTimer(this);
    cameraTimer->setInterval(100); 

    // 摄像头区域固定大小
    ui->cameraPanel->setFixedSize(1000, 800);



}

/* =========================================================
 * 信号槽绑定
 * ========================================================= */

void AttendanceWindow::initConnections()
{
    // 摄像头
    connect(ui->btnCameraToggle, &QPushButton::clicked,
        this, &AttendanceWindow::onCameraToggleClicked);
    connect(cameraTimer, &QTimer::timeout,
        this, &AttendanceWindow::updateFrame);


    // 考勤操作
    connect(ui->btnReset, &QPushButton::clicked,
        this, &AttendanceWindow::onResetClicked);
    connect(ui->btnExport, &QPushButton::clicked,
        this, &AttendanceWindow::onExportClicked);
    connect(ui->btnStop, &QPushButton::clicked,
        this, &AttendanceWindow::onStopAttendanceClicked);

    connect(ui->btnCollectFace, &QPushButton::clicked,
        this, &AttendanceWindow::onFaceExtractionClicked);

}

/* =========================================================
 * 摄像头相关
 * ========================================================= */

void AttendanceWindow::onCameraToggleClicked()
{
    if (!cameraOn) {
        if (!cap.open(0)) {
            QMessageBox::warning(this, "错误", "无法打开摄像头");
            return;
        }
        cameraOn = true;
        cameraTimer->start();
        ui->btnCameraToggle->setText("关闭摄像头");
        ui->labelStatus->setText("状态：摄像头已开启");
    }
    else {
        stopCamera();
    }
}

void AttendanceWindow::stopCamera()
{
    if (cameraTimer && cameraTimer->isActive())
        cameraTimer->stop();

    if (cap.isOpened())
        cap.release();

    cameraOn = false;
    ui->btnCameraToggle->setText("开启摄像头");
    ui->labelStatus->setText("状态：摄像头已关闭");

    // 关闭摄像头时显示默认图
    ui->labelCamera->setPixmap(
        defaultPixmap.scaled(
            ui->labelCamera->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        )
    );
}

void AttendanceWindow::updateFrame()
{
    if (!cap.isOpened())
        return;

    cv::Mat frame;
    cap >> frame;
    if (frame.empty())
        return;

    // ========== 人脸检测 ==========
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(gray, gray);

    std::vector<cv::Rect> faces;
    faceCascade.detectMultiScale(
        gray,
        faces,
        1.1,        // scaleFactor
        5,          // minNeighbors
        0,
        cv::Size(80, 80)  // 最小人脸尺寸
    );

    // ========== 画人脸框 ==========
    for (const auto& face : faces) {


        // ===== 人脸 ROI 预处理 =====
        cv::Mat faceROI = gray(face);
        cv::resize(faceROI, faceROI, cv::Size(200, 200));

        QString displayName = "Unknown";
        cv::Scalar color(0, 0, 255);   // 默认红框（Unknown）

        // ===== 只有在“识别器可用”时才做 predict =====
        if (recognizerReady && faceRecognizer) {

            int predictedLabel = -1;
            double confidence = 999.0;

            try {
                faceRecognizer->predict(faceROI, predictedLabel, confidence);


            }
            catch (...) {
                // 防御式处理：模型异常，直接当 Unknown
                stableCount = 0;
                stableName.clear();
                goto DRAW;
            }

            // ===== 判断是否可信 =====
            if (confidence < CONF_THRESHOLD &&
                labelToStudentId.count(predictedLabel))
            {
                QString currentName = labelToStudentId[predictedLabel];

                //QMessageBox::information(
                //    this,               // 父窗口部件
                //    "出勤信息",         // 消息框的标题
                //    "当前学生: " + currentName // 消息框的内容
                //);

                if (currentName == stableName) {
                    stableCount++;          // 同一个人，稳定度 +1
                }
                else {
                    stableName = currentName;
                    stableCount = 1;        // 新人，重新计数
                }
            }
            else {
                // 本帧识别失败
                stableCount = 0;
                stableName.clear();
            }

            // ===== 达到稳定帧数才显示姓名 =====

            if (stableCount >= STABLE_FRAMES && !stableName.isEmpty()) {                  // 如果识别成功

                displayName = stableName;
                color = cv::Scalar(0, 255, 0);

                //// ===== 防止重复触发 =====
                //if (autoSignedStudents.contains(stableName))
                //    goto DRAW;

                autoSignedStudents.insert(stableName);

                // ===== 查找是否在当前学生列表 =====
                int stuIndex = -1;
                for (int i = 0; i < students.size(); ++i) {
                    if (students[i].studentId == stableName) {
                        stuIndex = i;
                        break;
                    }
                }

                // ===== 不在名单中 =====
                if (stuIndex == -1) {
                    // ✅ 只在第一次出现时提示
                    if (!warnedNotInCourseStudents.contains(stableName)) {

                        warnedNotInCourseStudents.insert(stableName);

                        QMessageBox::warning(
                            this,
                            "提示",
                            "识别到的学生不在本课程名单中：\n" + stableName
                        );
                    }
                    goto DRAW;
                }

                // ===== 已签到过 =====
                if (students[stuIndex].studentStatus) {
                    goto DRAW;
                }

                // ===== 自动更新数据库 =====
                QSqlDatabase db = DatabaseManager::db();
                QSqlQuery query(db);

                query.prepare(R"(
                    UPDATE attendance
                    SET status = 1
                    WHERE course_id = :courseId
                      AND student_id = :studentId
                      AND week = :week
                )");

                query.bindValue(":courseId", courseId);
                query.bindValue(":studentId", students[stuIndex].studentId);
                query.bindValue(":week", week);

                if (!query.exec() || query.numRowsAffected() == 0) {
                    qDebug() << "Auto sign-in failed:" << query.lastError().text();
                    goto DRAW;
                }

                // ===== 更新内存数据 =====
                students[stuIndex].studentStatus = true;

                // ===== 刷新表格 UI+提示=====
                QMetaObject::invokeMethod(
                    this,
                    [=]() {
                        students[stuIndex].studentStatus = true;
                        updateAttendanceTable();

                        toastLabel->setText(
                            "签到成功：" + students[stuIndex].studentName
                        );
                        toastLabel->show();
                        toastTimer->start(3000);  //提示3秒
                    },
                    Qt::QueuedConnection
                );


                // ===== 1 秒悬浮提示 =====
                toastLabel->setText("签到成功：" + stableName);
                toastLabel->adjustSize();
                toastLabel->move(
                    (width() - toastLabel->width()) / 2,
                    height() - 120
                );
                toastLabel->show();
                toastTimer->start(1000);
            }

        }
        else {
            // ❗ 没有模型：永远 Unknown，但不清空 stable 状态（可选）
            stableCount = 0;
            stableName.clear();
        }

    DRAW:
        // ===== 绘制矩形框 =====
        cv::rectangle(frame, face, color, 2);

        // ===== 文本位置修正 =====
        int textX = face.x;
        int textY = face.y - 10;
        if (textY < 20) {
            textY = face.y + face.height + 25;
        }

        // ===== 绘制文字 =====
        cv::putText(
            frame,
            displayName.toStdString(),
            cv::Point(textX, textY),
            cv::FONT_HERSHEY_SIMPLEX,
            0.9,
            color,
            2
        );
    }



    // ========== 显示 ==========
    cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);

    QImage image(
        frame.data,
        frame.cols,
        frame.rows,
        frame.step,
        QImage::Format_RGB888
    );

    ui->labelCamera->setPixmap(
        QPixmap::fromImage(image).scaled(
            ui->labelCamera->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        )
    );
}




/* =========================================================
 * 学生表相关
 * ========================================================= */

void AttendanceWindow::updateAttendanceTable()
{
    ui->studentTable->setRowCount(students.size());

    for (int i = 0; i < students.size(); ++i) {
        const StudentInfo& s = students[i];

        // 学号
        ui->studentTable->setItem(
            i, 0, new QTableWidgetItem(s.studentId));

        // 姓名
        ui->studentTable->setItem(
            i, 1, new QTableWidgetItem(s.studentName));

        // 状态按钮
        QPushButton* statusBtn = new QPushButton;
        statusBtn->setText(s.studentStatus ? "已到" : "未到");
        statusBtn->setStyleSheet(
            s.studentStatus ? STYLE_PRESENT : STYLE_ABSENT
        );
        statusBtn->setCursor(Qt::PointingHandCursor);
        statusBtn->setFixedWidth(60);
        statusBtn->setFixedHeight(26);

        connect(statusBtn, &QPushButton::clicked, this, [=]() {
            bool oldStatus = students[i].studentStatus;
            bool newStatus = !oldStatus;

            QMessageBox::StandardButton reply =
                QMessageBox::question(
                    this,
                    "确认修改",
                    QString("确定将该学生状态修改为【%1】吗？")
                    .arg(newStatus ? "已到" : "未到"),
                    QMessageBox::Yes | QMessageBox::No
                );

            if (reply != QMessageBox::Yes)
                return;

            // ====== 1. 先更新数据库 ======
            QSqlDatabase db = DatabaseManager::db();
            QSqlQuery query(db);

            query.prepare(R"(
                UPDATE attendance
                SET status = :status
                WHERE course_id = :courseId
                  AND student_id = :studentId
                  AND week = :week
            )");


            query.bindValue(":status", newStatus ? 1 : 0);
            query.bindValue(":courseId", courseId);
            query.bindValue(":studentId", students[i].studentId);
            query.bindValue(":week", week);

            if (!query.exec()) {
                QMessageBox::critical(
                    this,
                    "数据库错误",
                    "考勤状态更新失败，请重试！\n\n"
                    + query.lastError().text()
                );
                qDebug() << "DB error:" << query.lastError().text();
                return;
            }

            // ====== 2. 数据库成功 → 更新内存数据 ======
            students[i].studentStatus = newStatus;

            // ====== 3. 更新按钮 UI ======
            statusBtn->setText(newStatus ? "已到" : "未到");
            statusBtn->setStyleSheet(
                newStatus ? STYLE_PRESENT : STYLE_ABSENT
            );
            });

        ui->studentTable->setCellWidget(i, 2, statusBtn);
    }
}



void AttendanceWindow::onResetClicked()
{
    QMessageBox::StandardButton reply =
        QMessageBox::question(
            this,
            "确认修改",
            QString("确定重置所有学生出勤状态吗？"),
            QMessageBox::Yes | QMessageBox::No
        );

    if (reply != QMessageBox::Yes)
        return;


    /* ===== 1. 更新数据库：status 统一设为 0 ===== */
    QSqlDatabase db = DatabaseManager::db();  //先加载数据库
    QSqlQuery query(db);
    query.prepare(
        "UPDATE attendance "
        "SET status = 0 "
        "WHERE course_id = :courseId "
        "AND week = :week"
    );

    query.bindValue(":courseId", courseId);
    query.bindValue(":week", week);

    if (!query.exec()) {
        qWarning() << "Reset attendance failed:"
            << query.lastError().text();

        QMessageBox::warning(this, "错误", "数据库更新失败");
        return;
    }

    /* ===== 2. 同步内存数据 ===== */
    for (auto& s : students)
        s.studentStatus = false;

    /* ===== 3. 更新 UI ===== */
    ui->labelStatus->setText("状态：考勤已重置（全部设为未到）");
    updateAttendanceTable();
}


void AttendanceWindow::onExportClicked()
{
    QString path = QFileDialog::getSaveFileName(
        this,
        "导出考勤结果",
        QDir::homePath(),
        "CSV 文件 (*.csv)"
    );

    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法写入文件");
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    out << QString(QChar(0xFEFF));

    /* ===== 表头 ===== */
    out << "学生姓名,学生学号";
    for (int w = 1; w <= 18; ++w) {
        out << ",第" << w << "周";
    }
    out << "\n";

    /* ===== 数据行 ===== */
    for (const auto& s : students) {

        out << s.studentName << ",=\"" << s.studentId << "\"";


        for (int w = 1; w <= 18; ++w) {
            int status;
            if (queryAttendance(courseId, s.studentId, w, status)) {
                out << "," << status;   // 有数据：0 或 1
            }
            else {
                out << ",";             // 无数据：空
            }
        }

        out << "\n";
    }

    file.close();
    QMessageBox::information(this, "完成", "考勤结果已导出（CSV）");
}


/* =========================================================
 * 结束考勤
 * ========================================================= */

void AttendanceWindow::onStopAttendanceClicked()
{
    stopCamera();

    FaceAttendanceSystem* mainWin = new FaceAttendanceSystem();
    mainWin->show();

    this->close();   
}




/* ======================================================== =
*  进入人脸采集界面
* ======================================================== = */
void AttendanceWindow::onFaceExtractionClicked()
{
    FaceExtraction* win =
        new FaceExtraction(nullptr, courseId, courseName, week, students);
    win->show();
    this->close();
}


void AttendanceWindow::loadLabelMap()
{
    labelToStudentId.clear();

    QFile file("D:/myDownload/vs2022/vs2022Project/FaceAttendanceSystem2/x64/Release/label_map.txt");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法打开 label_map.txt");
        return;
    }

    QTextStream in(&file);   // Qt 6 默认 UTF-8

    while (!in.atEnd()) {

        QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;

        // 用空格分割（自动忽略多余空格）
        QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() < 2)
            continue;

        bool ok = false;
        int label = parts[0].toInt(&ok);
        if (!ok)
            continue;

        QString studentId = parts[1].trimmed();
        if (studentId.isEmpty())
            continue;

        labelToStudentId[label] = studentId;
    }

    file.close();

    //// 调试确认（用完可删）
    //QString msg = "加载 labelMap：\n";
    //for (const auto& p : labelToStudentId) {
    //    msg += QString("label=%1 , studentId=%2\n")
    //        .arg(p.first)
    //        .arg(p.second);
    //}
    //QMessageBox::information(this, "LabelMap Debug", msg);
}

// 导出csv时查询出勤记录

bool AttendanceWindow::queryAttendance(
    const QString& courseId,
    const QString& studentId,
    int week,
    int& status
) {
    /*QSqlQuery query;*/
    QSqlDatabase db = DatabaseManager::db();  //先加载数据库
    QSqlQuery query(db);

    query.prepare(
        "SELECT status "
        "FROM attendance "
        "WHERE course_id = :courseId "
        "  AND student_id = :studentId "
        "  AND week = :week"
    );

    query.bindValue(":courseId", courseId);
    query.bindValue(":studentId", studentId);
    query.bindValue(":week", week);

    if (!query.exec()) {
        qWarning() << "queryAttendance failed:"
            << query.lastError().text();
        return false;
    }

    if (query.next()) {
        status = query.value(0).toInt();  // 0 / 1
        return true;
    }

    // 没有这条考勤记录
    return false;
}



//弹窗显示人脸检测结果

/*static bool shown = false;
if (!shown) {
    QString msg;
    msg += QString("predictedLabel = %1\n").arg(predictedLabel);
    msg += QString("confidence = %1\n").arg(confidence);

    msg += "labelMap keys = ";
    for (const auto& p : labelToStudentId) {
        msg += QString::number(p.first) + " ";
    }
    msg += "\n";

    msg += QString("map contains predictedLabel = %1")
        .arg(labelToStudentId.count(predictedLabel) ? "YES" : "NO");

    QMessageBox::information(this, "Debug: Predict", msg);
    shown = true;
}*/

/* ======================================================== =
*  加载人脸数据库
* ======================================================== = */
//void AttendanceWindow::loadFaceDatabase() {
//
//    QString faceDirPath = "D:/myDownload/vs2022/vs2022Project/FaceAttendanceSystem2/x64/Release/face";
//
//    QDir dir(faceDirPath);
//    QStringList filters;
//    filters << "*.png" << "*.jpg";
//    dir.setNameFilters(filters);
//
//    QFileInfoList fileList = dir.entryInfoList();
//
//    std::vector<cv::Mat> images;
//    std::vector<int> labels;
//
//    int label = 0;
//
//    for (const QFileInfo& fileInfo : fileList) {
//        QString filePath = fileInfo.absoluteFilePath();
//        QString studentId = fileInfo.baseName(); // 文件名 = 学号
//
//        cv::Mat img = cv::imread(filePath.toStdString());
//        if (img.empty())
//            continue;
//
//        // 转灰度
//        cv::Mat gray;
//        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
//
//        // 人脸检测（只取第一张脸）
//        std::vector<cv::Rect> faces;
//        faceCascade.detectMultiScale(gray, faces, 1.1, 5);
//
//        if (faces.empty())
//            continue;
//
//        cv::Mat faceROI = gray(faces[0]);
//        cv::resize(faceROI, faceROI, cv::Size(200, 200));
//
//        images.push_back(faceROI);
//        labels.push_back(label);
//
//        labelToStudentId[label] = studentId;
//        label++;
//    }
//
//    // 创建并训练 LBPH 模型
//    faceRecognizer = cv::face::LBPHFaceRecognizer::create(
//        2, 8, 8, 8, 150.0
//    );
//
//    faceRecognizer->train(images, labels);
//
//}

//void AttendanceWindow::loadFaceDatabase()
//{
//    QString faceRootPath =
//        "D:/myDownload/vs2022/vs2022Project/FaceAttendanceSystem2/x64/Release/face";
//
//    QDir rootDir(faceRootPath);
//    if (!rootDir.exists()) {
//        qDebug() << "人脸库路径不存在";
//        return;
//    }
//
//    // 获取所有“学号文件夹”
//    QFileInfoList studentDirs =
//        rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
//
//    std::vector<cv::Mat> images;
//    std::vector<int> labels;
//
//    int label = 0;
//
//    for (const QFileInfo& studentDirInfo : studentDirs) {
//
//        QString studentId = studentDirInfo.fileName();  // 文件夹名 = 学号
//        QDir studentDir(studentDirInfo.absoluteFilePath());
//
//        // 读取该学生的所有图片
//        QStringList filters;
//        filters << "*.jpg" << "*.png" << "*.jpeg";
//        QFileInfoList imageFiles =
//            studentDir.entryInfoList(filters, QDir::Files);
//
//        if (imageFiles.isEmpty())
//            continue;
//
//        for (const QFileInfo& imgInfo : imageFiles) {
//
//            cv::Mat img = cv::imread(imgInfo.absoluteFilePath().toStdString());
//            if (img.empty())
//                continue;
//
//            // 转灰度
//            cv::Mat gray;
//            cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
//
//            // 人脸检测
//            std::vector<cv::Rect> faces;
//            faceCascade.detectMultiScale(gray, faces, 1.1, 5);
//
//            if (faces.empty())
//                continue;
//
//            // 只取第一张脸
//            cv::Mat faceROI = gray(faces[0]);
//            cv::resize(faceROI, faceROI, cv::Size(200, 200));
//
//            images.push_back(faceROI);
//            labels.push_back(label);
//        }
//
//        // 建立 label → studentId 映射
//        labelToStudentId[label] = studentId;
//        label++;
//    }
//
//    if (images.empty()) {
//        qDebug() << "未加载到任何有效人脸样本";
//        return;
//    }
//
//    // 创建并训练 LBPH 模型
//    faceRecognizer = cv::face::LBPHFaceRecognizer::create(
//        2,      // radius
//        8,      // neighbors
//        8, 8,   // grid_x, grid_y
//        150.0   // threshold
//    );
//
//    faceRecognizer->train(images, labels);
//
//    // ================= 保存模型 =================
//    QString modelPath =
//        "D:/myDownload/vs2022/vs2022Project/FaceAttendanceSystem2/x64/Release/lbph_model.xml";
//
//    faceRecognizer->save(modelPath.toStdString());
//
//    qDebug() << "人脸模型训练完成并保存：" << modelPath;
//}


