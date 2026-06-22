#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QPushButton>
#include "QFileDialog.h"
#include "dialog.h"

#include <vtkNew.h>
#include <vtkCylinderSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkCamera.h>
#include <vtkSTLReader.h>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->treeView->addAction(ui->actionOpen_File);
    ui->treeView->addAction(ui->actionItemOptions);
    ui->treeView->setContextMenuPolicy(Qt::ActionsContextMenu);

    // 在 MainWindow::MainWindow 中
    ui->treeView->addAction(ui->actionAdd_Item); // 添加 Action
    ui->treeView->setContextMenuPolicy(Qt::ActionsContextMenu); // 确保右键菜单策略已开启
ui->treeView->addAction(ui->actionDelete_Item);
    /*Create / allocate the ModelList */
    this->partList =new ModelPartList("PartsList");
    /*Link it to the treeview in the GUI */
    ui->treeView->setModel(this->partList);
    /*Manually create a model tree -there amuch better andmore flexible ways of doing this ,e.g. with nested functions. This is just a quick example as a starting point. */
    ModelPart *rootItem= this->partList->getRootItem();
    /*Add 3 top level items */
    for (int i = 0; i < 3; i++) {
        /*Create strings for both data columns */
        QString name=QString("TopLevel %1").arg(i);
        QString visible("true");
        /*Create child item */
        ModelPart *childItem=new ModelPart({ name, visible });
        /*Append to tree top-level */
        rootItem->appendChild(childItem);
        /*Add 5 sub-items */
        for (int j = 0; j < 5; j++) {
            QString name=QString("Item%1,%2").arg(i).arg(j);
            QString visible("true");
            ModelPart *childChildItem=new ModelPart({ name, visible });
            /*Append to parent */
            childItem->appendChild(childChildItem);
        }
    }

    //connect(ui->actionItemOptions, &QAction::triggered, this, &MainWindow::on_actionItemOptions_triggered);

    connect( ui->pushButton , &QPushButton :: released , this , &MainWindow :: handleButton );
    connect( ui->pushButton_2 , &QPushButton :: released , this , &MainWindow :: handleButton );

    connect( this , &MainWindow:: statusUpdateMessage , ui->statusBar , &QStatusBar :: showMessage );

    connect(ui->treeView, &QTreeView::clicked, this, &MainWindow::handleTreeClicked);

    /* --- 将以下内容添加到 MainWindow 构造函数 --- */

    /* 将渲染窗口与 Qt 挂件（widget）关联 */
    renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    ui->vtkWidget->setRenderWindow(renderWindow);

    /* 添加渲染器 */
    renderer = vtkSmartPointer<vtkRenderer>::New();
    renderWindow->AddRenderer(renderer);

    /* 创建一个对象并添加到渲染器中（稍后会更改为显示 CAD 模型） */
    /* 这里直接复制并粘贴之前的圆柱体示例 */
    /* 这将创建一个具有 8 个侧面的多边形圆柱体模型（即实践中的八角柱） */
    vtkNew<vtkCylinderSource> cylinder;
    cylinder->SetResolution(8);

    // Mapper 负责将几何图形推送到图形库中
    // 如果定义了标量或其他属性，它也可以进行颜色映射
    vtkNew<vtkPolyDataMapper> cylinderMapper;
    cylinderMapper->SetInputConnection(cylinder->GetOutputPort());

    // Actor 是一个组合机制：除了几何图形 (mapper) 之外，
    // 它还具有属性、变换矩阵和/或纹理贴图。
    // 这里我们设置它的颜色，并让它绕 X 和 Y 轴旋转
    vtkNew<vtkActor> cylinderActor;
    cylinderActor->SetMapper(cylinderMapper);
    cylinderActor->GetProperty()->SetColor(1.0, 0.0, 0.35); // 设置为红色系
    cylinderActor->RotateX(30.0);
    cylinderActor->RotateY(-45.0);

    renderer->AddActor(cylinderActor);

    /* 重置摄像头（可能需要放入一个在模型更改时调用的独立函数中） */
    renderer->ResetCamera();
    renderer->GetActiveCamera()->Azimuth(30);
    renderer->GetActiveCamera()->Elevation(30);
    renderer->ResetCameraClippingRange();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::handleButton() {
    // 1. 获取当前 TreeView 中选中的索引
    QModelIndex index = ui->treeView->currentIndex();

    // 2. 准备新节点的数据（默认名称和可见性）
    QList<QVariant> newData;
    newData << "New Part" << "Visible";

    // 3. 调用你的 ModelPartList::appendChild 方法
    // 注意：如果 index 无效（未选中），你的代码会自动将其添加到 rootItem 下
    QModelIndex newChildIndex = partList->appendChild(index, newData);

    // 4. 提升用户体验：自动展开父节点并选中新节点
    if (newChildIndex.isValid()) {
        ui->treeView->expand(index); // 展开父节点以便看到新项
        ui->treeView->setCurrentIndex(newChildIndex); // 选中新创建的项
    }

    emit statusUpdateMessage(QString("Added new child to ") +
                                 (index.isValid() ? static_cast<ModelPart*>(index.internalPointer())->data(0).toString() : "Root"), 0);
}
void MainWindow:: handleTreeClicked() {
    /* Get the index of the selected item */
    QModelIndex index = ui->treeView->currentIndex () ;
    /* Get a pointer to the item from the index */
    ModelPart *selectedPart = static_cast<ModelPart*>(index . internalPointer ()) ;
    /* In this case , we will retrieve the name string from the internal QVariant data array */
    QString text = selectedPart->data(0).toString () ;
    emit statusUpdateMessage(QString("The selected item is : ")+text , 0) ;
}

void MainWindow::on_actionOpen_File_triggered() {
    QString fileName = QFileDialog::getOpenFileName(
        this, tr("Open STL File"), "C:\\", tr("STL Files (*.stl)")
        );

    if (!fileName.isEmpty()) {
        QModelIndex index = ui->treeView->currentIndex();
        if (index.isValid()) {
            ModelPart* selectedPart = static_cast<ModelPart*>(index.internalPointer());

            // 1. 设置节点名称（仅显示文件名）
            selectedPart->set(0, QFileInfo(fileName).fileName());

            // 2. 直接传递 QString (修正点：去掉 .toStdString())
            selectedPart->loadSTL(fileName);

            // 3. 通知 View 数据已更改
            emit partList->dataChanged(index, index);

            // 4. 更新渲染器，显示新加载的模型
            updateRender();
        }
    }
}

void MainWindow::on_actionItemOptions_triggered() {
    QModelIndex index = ui->treeView->currentIndex();
    if (!index.isValid()) {
        emit statusUpdateMessage("Warning: No item selected", 0);
        return;
    }

    ModelPart* selectedPart = static_cast<ModelPart*>(index.internalPointer());
    Dialog dialog(this);

    dialog.setName(selectedPart->data(0).toString());
    dialog.setRGB(selectedPart->getColourR(), selectedPart->getColourG(), selectedPart->getColourB());
    dialog.setVisible(selectedPart->visible());

    QString openMsg = QString("Opening options for item: %1").arg(selectedPart->data(0).toString());
    emit statusUpdateMessage(openMsg, 0);

    if (dialog.exec() == QDialog::Accepted) {
        selectedPart->set(0, dialog.getName());

        int r, g, b;
        dialog.getRGB(r, g, b);
        selectedPart->setColour(r, g, b);
        selectedPart->setVisible(dialog.getVisible());

        emit ui->treeView->model()->dataChanged(index, index);

        emit statusUpdateMessage("Changes saved successfully.", 0);
    } else {
        emit statusUpdateMessage("Edit cancelled by user.", 0);
    }
}

void MainWindow::updateRender() {
    // 1. 清除渲染器中现有的所有模型
    renderer->RemoveAllViewProps();

    // 2. 从根节点开始递归遍历树
    // 获取 ModelPartList 的根索引 (row 0, col 0)
    updateRenderFromTree(partList->index(0, 0, QModelIndex()));

    // 3. 刷新渲染窗口
    renderWindow->Render();
}

void MainWindow::updateRenderFromTree(const QModelIndex& index) {
    if (index.isValid()) {
        ModelPart* selectedPart = static_cast<ModelPart*>(index.internalPointer());

        // 1. 获取该部件的 Actor
        vtkSmartPointer<vtkActor> actor = selectedPart->getActor();

        // 2. 如果 Actor 存在且部件设为可见，则添加到渲染器
        if (actor) {
            // 根据 visible 属性决定是否添加或设置透明度
            if (selectedPart->visible()) {
                actor->VisibilityOn();
            } else {
                actor->VisibilityOff();
            }
            renderer->AddActor(actor);

            // 设置颜色（从 ModelPart 获取）
            actor->GetProperty()->SetColor(
                selectedPart->getColourR() / 255.0,
                selectedPart->getColourG() / 255.0,
                selectedPart->getColourB() / 255.0
                );
        }
    }

    // 3. 递归处理所有子节点
    if (partList->hasChildren(index)) {
        for (int i = 0; i < partList->rowCount(index); i++) {
            updateRenderFromTree(partList->index(i, 0, index));
        }
    }
}

void MainWindow::on_actionAdd_Item_triggered() {
    // 1. 获取当前选中的索引
    QModelIndex index = ui->treeView->currentIndex();

    // 2. 准备默认数据
    QList<QVariant> newData;
    newData << "New Sub-Part" << "true";

    // 3. 调用 ModelPartList 的 appendChild
    // 如果 index 无效（未选中），你的代码会自动添加到根节点下
    QModelIndex newChildIndex = partList->appendChild(index, newData);

    // 4. 交互优化：自动展开并选中新项
    if (newChildIndex.isValid()) {
        ui->treeView->expand(index); // 展开当前选中的父项
        ui->treeView->setCurrentIndex(newChildIndex); // 选中新创建的子项
    }

    emit statusUpdateMessage(tr("Added new child item."), 0);
}

void MainWindow::on_actionDelete_Item_triggered() {
    QModelIndex index = ui->treeView->currentIndex();
    if (!index.isValid()) {
        emit statusUpdateMessage("Warning: No item selected for deletion", 0);
        return;
    }

    ModelPart* selectedPart = static_cast<ModelPart*>(index.internalPointer());
    QString name = selectedPart->data(0).toString();

    // 确认对话框
    auto reply = QMessageBox::question(this, "Delete Confirmation",
                                       QString("Are you sure you want to delete '%1' and all its children?").arg(name),
                                       QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        // 执行模型层的删除
        if (partList->removePart(index)) {
            // 关键：删除后必须刷新渲染器
            updateRender();
            emit statusUpdateMessage(QString("Item '%1' deleted.").arg(name), 0);
        }
    }
}
