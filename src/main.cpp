/**
 * @file main.cpp
 * @brief Application entry point for the Qt STL assembly and VR viewer.
 */

#include "mainwindow.h"

#include <QApplication>

/**
 * @brief Starts the Qt event loop and displays the main window.
 * @param argc Command-line argument count supplied by the operating system.
 * @param argv Command-line argument values supplied by the operating system.
 * @return Qt application exit code.
 */
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
