#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "ModelPartList.h"
#include "ModelPart.h"

#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkGenericOpenGLRenderWindow.h>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void handleButton();
    void handleTreeClicked();
    void on_actionOpen_File_triggered();
    void on_actionItemOptions_triggered();
void on_actionAdd_Item_triggered();
    void on_actionDelete_Item_triggered();
signals:
    void statusUpdateMessage(const QString & message, int timeout);

private:
    // --- Exercise 4 新增：渲染更新函数 ---
    void updateRender(); // 清理并重新启动渲染流程
    void updateRenderFromTree(const QModelIndex& index); // 递归遍历树节点并添加 Actor

private:
    Ui::MainWindow *ui;
    ModelPartList* partList;

    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> renderWindow;
};

#endif // MAINWINDOW_H
