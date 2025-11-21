#include "main_window.h"
#include <QApplication>
#include <QFile>
int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    QCoreApplication::setApplicationName("VeloCAD");
 

    qRegisterMetaType<Shape>("Shape");
  
    MainWindow w;

    // 颜色样式
    QFile style_file(":/style.qss");
    style_file.open(QFile::ReadOnly);
    if (style_file.isOpen()) {
        const auto style_str = style_file.readAll();
        a.setStyleSheet(style_str);
        style_file.close();
    }
    // 显示主窗口
        w.show();
        QApplication::exec();
    
}