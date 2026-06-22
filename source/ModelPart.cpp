/** @file ModelPart.cpp
  *
  * EEEE2076 - Software Engineering & VR Project
  *
  * Template for model parts that will be added as treeview items
  *
  * P Evans 2022
  */

#include "ModelPart.h"

/* 取消注释并添加必要的 VTK 头文件 */
#include <vtkSmartPointer.h>
#include <vtkPolyDataMapper.h>
#include <vtkSTLReader.h>
#include <vtkActor.h>
#include <vtkProperty.h>

ModelPart::ModelPart(const QList<QVariant>& data, ModelPart* parent)
    : m_itemData(data), m_parentItem(parent) {

    this->isVisible = true;

    if (m_itemData.size() > 1) {
        m_itemData.replace(1, "Visible");
    }

    /* 初始化指针防止悬挂指针崩溃 */
    file = nullptr;
    mapper = nullptr;
    actor = nullptr;
}

ModelPart::~ModelPart() {
    qDeleteAll(m_childItems);
}

void ModelPart::appendChild( ModelPart* item ) {
    item->m_parentItem = this;
    m_childItems.append(item);
}

ModelPart* ModelPart::child( int row ) {
    if (row < 0 || row >= m_childItems.size())
        return nullptr;
    return m_childItems.at(row);
}

int ModelPart::childCount() const {
    return m_childItems.count();
}

int ModelPart::columnCount() const {
    return m_itemData.count();
}

QVariant ModelPart::data(int column) const {
    if (column < 0 || column >= m_itemData.size())
        return QVariant();
    return m_itemData.at(column);
}

void ModelPart::set(int column, const QVariant &value) {
    if (column < 0 || column >= m_itemData.size())
        return;
    m_itemData.replace(column, value);
}

ModelPart* ModelPart::parentItem() {
    return m_parentItem;
}

int ModelPart::row() const {
    if (m_parentItem)
        return m_parentItem->m_childItems.indexOf(const_cast<ModelPart*>(this));
    return 0;
}

void ModelPart::setColour(const unsigned char R, const unsigned char G, const unsigned char B) {
    m_r = R;
    m_g = G;
    m_b = B;
    /* 如果 actor 已经存在，立即更新颜色 */
    if (actor) {
        actor->GetProperty()->SetColor(R / 255.0, G / 255.0, B / 255.0);
    }
}

unsigned char ModelPart::getColourR() { return m_r; }
unsigned char ModelPart::getColourG() { return m_g; }
unsigned char ModelPart::getColourB() { return m_b; }

void ModelPart::setVisible(bool isVisible) {
    this->isVisible = isVisible;
    if (m_itemData.size() > 1) {
        m_itemData.replace(1, isVisible ? "Visible" : "Hidden");
    }
    /* 同步更新 VTK Actor 的可见性 */
    if (actor) {
        actor->SetVisibility(isVisible);
    }
}

bool ModelPart::visible() {
    return isVisible;
}

/** * Exercise 4: 实现 STL 加载流水线
 */
void ModelPart::loadSTL( QString fileName ) {
    // 1. 使用 vtkSTLReader 加载文件
    file = vtkSmartPointer<vtkSTLReader>::New();
    file->SetFileName(fileName.toStdString().c_str());
    file->Update(); // 强制读取数据

    // 2. 初始化映射器 (Mapper)
    mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(file->GetOutputPort());

    // 3. 初始化演员 (Actor) 并建立链接
    actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);

    // 设置初始颜色和可见性
    actor->GetProperty()->SetColor(m_r / 255.0, m_g / 255.0, m_b / 255.0);
    actor->SetVisibility(this->isVisible);
}

/**
 * Exercise 5/VR: 创建用于 VR 的第二个 Actor
 */
vtkActor* ModelPart::getNewActor() {
    if (!file) return nullptr;

    /* 1. 创建新 Mapper (共享同一个文件源) */
    vtkSmartPointer<vtkPolyDataMapper> vrMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    vrMapper->SetInputConnection(file->GetOutputPort());

    /* 2. 创建新 Actor 并链接 */
    vtkActor* vrActor = vtkActor::New();
    vrActor->SetMapper(vrMapper);

    /* 3. 共享属性：这样在 GUI 修改颜色时，VR 也会同步 */
    if (actor) {
        vrActor->SetProperty(actor->GetProperty());
    }

    return vrActor;
}

void ModelPart::removeChild(int row) {
    if (row >= 0 && row < m_childItems.size()) {
        // 1. 从 QList 中移除该指针并获取它
        ModelPart* item = m_childItems.takeAt(row);

        // 2. 物理删除内存
        // 这会触发 ModelPart 的析构函数，从而递归删除它下面的所有子项
        delete item;
    }
}
