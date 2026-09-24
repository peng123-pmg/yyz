#include "admindialog.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QLabel>
#include <QComboBox>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QPixmap>
#include <QTimer>

#include "facerecognizerservice.h"

namespace {
const int kSampleCount = 10;        // 自动采集的样本数量
const int kCaptureIntervalMs = 400; // 每张照片之间的采集间隔
}

AdminDialog::AdminDialog(FaceRecognizerService *service,
                         std::function<QImage()> frameProvider,
                         QWidget *parent)
    : QDialog(parent), m_service(service), m_frameProvider(frameProvider)
{
    setWindowTitle(QStringLiteral("成员管理"));
    setMinimumSize(680, 480);
    setStyleSheet(QStringLiteral(R"(
QLabel#previewLabel {
    background-color: #0f1114;
    border: 1px solid #2b2f35;
    border-radius: 8px;
    color: #6b7280;
}
QPushButton {
    background-color: #33373d;
    color: #e8eaed;
    border: none;
    border-radius: 6px;
    padding: 8px 18px;
    font-weight: bold;
    font-size: 13px;
}
QPushButton:hover {
    background-color: #3c4047;
}
QPushButton:pressed {
    background-color: #2b2f35;
}
QPushButton:disabled {
    background-color: #26292e;
    color: #6b7280;
}
QPushButton#primaryButton {
    background-color: #2ea56c;
    color: #ffffff;
}
QPushButton#primaryButton:hover {
    background-color: #35b878;
}
QPushButton#primaryButton:pressed {
    background-color: #278e5d;
}
QPushButton#dangerButton {
    background-color: #e05252;
    color: #ffffff;
}
QPushButton#dangerButton:hover {
    background-color: #ea6363;
}
QPushButton#dangerButton:pressed {
    background-color: #c84747;
}
)"));

    /* 左侧：成员列表 */
    m_memberTable = new QTableWidget(this);
    m_memberTable->setColumnCount(3);
    m_memberTable->setHorizontalHeaderLabels(
        {QStringLiteral("ID"), QStringLiteral("姓名"), QStringLiteral("样本数")});
    m_memberTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_memberTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_memberTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_memberTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_memberTable->verticalHeader()->setVisible(false);

    QPushButton *deleteButton = new QPushButton(QStringLiteral("删除选中成员"), this);
    deleteButton->setObjectName(QStringLiteral("dangerButton"));
    connect(deleteButton, &QPushButton::clicked,
            this, &AdminDialog::onDeleteSelected);

    QVBoxLayout *leftLayout = new QVBoxLayout;
    leftLayout->addWidget(new QLabel(QStringLiteral("已注册成员"), this));
    leftLayout->addWidget(m_memberTable, 1);
    leftLayout->addWidget(deleteButton);

    /* 右侧：注册新成员 */
    QLabel *nameLabel = new QLabel(QStringLiteral("姓名"), this);
    m_nameCombo = new QComboBox(this);
    for (int i = 1; i <= 9; ++i) {
        m_nameCombo->addItem(QStringLiteral("成员%1").arg(i));
    }
    m_nameCombo->setCurrentIndex(0);

    m_previewLabel = new QLabel(QStringLiteral("摄像头未开启"), this);
    m_previewLabel->setObjectName(QStringLiteral("previewLabel"));
    m_previewLabel->setFixedSize(220, 165);
    m_previewLabel->setAlignment(Qt::AlignCenter);

    m_previewTimer = new QTimer(this);
    m_previewTimer->setInterval(33);
    connect(m_previewTimer, &QTimer::timeout,
            this, &AdminDialog::onPreviewTimeout);
    m_previewTimer->start();

    m_captureButton = new QPushButton(QStringLiteral("开始采集"), this);
    m_captureButton->setObjectName(QStringLiteral("primaryButton"));
    connect(m_captureButton, &QPushButton::clicked,
            this, &AdminDialog::onStartCapture);

    m_captureCountLabel = new QLabel(
        QStringLiteral("已采集 0/%1 张").arg(kSampleCount), this);
    m_captureCountLabel->setAlignment(Qt::AlignCenter);

    m_captureTimer = new QTimer(this);
    m_captureTimer->setInterval(kCaptureIntervalMs);
    connect(m_captureTimer, &QTimer::timeout,
            this, &AdminDialog::onCaptureTimeout);

    QGroupBox *registerGroup = new QGroupBox(QStringLiteral("注册新成员"), this);
    QVBoxLayout *registerLayout = new QVBoxLayout(registerGroup);
    registerLayout->setSpacing(10);
    registerLayout->addWidget(nameLabel);
    registerLayout->addWidget(m_nameCombo);
    registerLayout->addWidget(m_previewLabel, 0, Qt::AlignHCenter);
    registerLayout->addWidget(m_captureButton);
    registerLayout->addWidget(m_captureCountLabel);

    QWidget *rightWidget = new QWidget(this);
    rightWidget->setFixedWidth(250);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(registerGroup);
    rightLayout->addStretch(1);

    QPushButton *returnButton = new QPushButton(QStringLiteral("返回人脸识别界面"), this);
    connect(returnButton, &QPushButton::clicked, this, [this]() { close(); });

    QHBoxLayout *contentLayout = new QHBoxLayout;
    contentLayout->setSpacing(16);
    contentLayout->addLayout(leftLayout, 1);
    contentLayout->addWidget(rightWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);
    mainLayout->addLayout(contentLayout, 1);
    mainLayout->addWidget(returnButton);

    refreshMemberTable();
}

void AdminDialog::refreshMemberTable()
{
    if (!m_service)
        return;

    const QList<MemberInfo> members = m_service->memberList();
    m_memberTable->setRowCount(members.size());

    for (int row = 0; row < members.size(); ++row) {
        const MemberInfo &info = members.at(row);

        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(info.id));
        idItem->setData(Qt::UserRole, info.id);
        m_memberTable->setItem(row, 0, idItem);

        m_memberTable->setItem(row, 1, new QTableWidgetItem(info.name));
        m_memberTable->setItem(row, 2,
            new QTableWidgetItem(QString::number(info.sampleCount)));
    }
}

void AdminDialog::onPreviewTimeout()
{
    const QImage frame = m_frameProvider();
    if (frame.isNull()) {
        m_previewLabel->clear();
        m_previewLabel->setText(QStringLiteral("摄像头未开启"));
        return;
    }

    /* 显示彩色实时画面，供管理员对准人脸 */
    const QPixmap scaled = QPixmap::fromImage(frame).scaled(
        m_previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_previewLabel->setPixmap(scaled);
}

void AdminDialog::onStartCapture()
{
    const QString name = m_nameCombo->currentText();

    const QList<MemberInfo> members = m_service->memberList();
    for (const MemberInfo &member : members) {
        if (member.name == name) {
            QMessageBox::warning(this, QStringLiteral("提示"),
                                 QStringLiteral("该姓名已存在，请使用其它姓名"));
            return;
        }
    }

    m_pendingFaces.clear();
    m_captureCountLabel->setText(
        QStringLiteral("已采集 0/%1 张").arg(kSampleCount));
    m_captureButton->setEnabled(false);
    m_captureTimer->start();
}

void AdminDialog::onCaptureTimeout()
{
    if (!m_service)
        return;

    const QImage frame = m_frameProvider();
    if (frame.isNull()) {
        m_captureCountLabel->setText(QStringLiteral("摄像头未开启"));
        return;
    }

    const cv::Mat face = m_service->extractFaceRoi(frame);
    if (face.empty()) {
        m_captureCountLabel->setText(
            QStringLiteral("未检测到人脸 (%1/%2)")
                .arg(m_pendingFaces.size()).arg(kSampleCount));
        return;
    }

    m_pendingFaces.append(face);
    m_captureCountLabel->setText(
        QStringLiteral("已采集 %1/%2 张")
            .arg(m_pendingFaces.size()).arg(kSampleCount));

    if (m_pendingFaces.size() >= kSampleCount) {
        m_captureTimer->stop();
        finishRegistration();
    }
}

void AdminDialog::finishRegistration()
{
    const QString name = m_nameCombo->currentText();

    QString error;
    if (!m_service->registerMember(name, m_pendingFaces, &error)) {
        QMessageBox::warning(this, QStringLiteral("注册失败"), error);
        m_captureButton->setEnabled(true);
        return;
    }

    m_pendingFaces.clear();
    m_captureCountLabel->setText(
        QStringLiteral("已采集 0/%1 张").arg(kSampleCount));
    refreshMemberTable();
    m_captureButton->setEnabled(true);

    QMessageBox::information(this, QStringLiteral("成功"),
        QStringLiteral("成员「%1」注册成功").arg(name));
}

void AdminDialog::onDeleteSelected()
{
    const int row = m_memberTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请先选择要删除的成员"));
        return;
    }

    const int memberId = m_memberTable->item(row, 0)->data(Qt::UserRole).toInt();
    const QString name = m_memberTable->item(row, 1)->text();

    if (QMessageBox::question(this, QStringLiteral("确认"),
            QStringLiteral("确定删除成员「%1」及其所有人脸样本吗？").arg(name))
        != QMessageBox::Yes) {
        return;
    }

    QString error;
    if (!m_service->deleteMember(memberId, &error)) {
        QMessageBox::warning(this, QStringLiteral("删除失败"), error);
        return;
    }

    refreshMemberTable();
}
