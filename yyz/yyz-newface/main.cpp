#include <QApplication>
#include <QPalette>
#include <QColor>
#include <QMetaType>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    /* 注册 FaceResult 类型，供跨线程（采集线程 -> 界面线程）的队列信号使用 */
    qRegisterMetaType<FaceResult>("FaceResult");

    /* Fusion 风格 + 深色配色，保证 QComboBox 等控件渲染一致 */
    app.setStyle(QStringLiteral("Fusion"));

    QPalette pal;
    pal.setColor(QPalette::Window, QColor(24, 26, 29));
    pal.setColor(QPalette::WindowText, QColor(232, 234, 237));
    pal.setColor(QPalette::Base, QColor(34, 37, 42));
    pal.setColor(QPalette::AlternateBase, QColor(31, 34, 38));
    pal.setColor(QPalette::Text, QColor(232, 234, 237));
    pal.setColor(QPalette::Button, QColor(51, 55, 61));
    pal.setColor(QPalette::ButtonText, QColor(232, 234, 237));
    pal.setColor(QPalette::Highlight, QColor(59, 130, 246));
    pal.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    pal.setColor(QPalette::ToolTipBase, QColor(34, 37, 42));
    pal.setColor(QPalette::ToolTipText, QColor(232, 234, 237));
    app.setPalette(pal);

    MainWindow w;
    w.show();
    return app.exec();
}
