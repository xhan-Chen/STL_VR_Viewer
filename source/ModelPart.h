/** @file ModelPart.h
 *
 * EEEE2076 - Software Engineering & VR Project
 *
 * Template for model parts that will be added as treeview items
 *
 * P Evans 2022
 */

#ifndef VIEWER_MODELPART_H
#define VIEWER_MODELPART_H

#include <QString>
#include <QList>
#include <QVariant>

/* VTK headers - 已经取消注释以支持 Exercise 4 */
#include <vtkSmartPointer.h>
#include <vtkPolyDataMapper.h> // 使用具体的 Mapper
#include <vtkActor.h>
#include <vtkSTLReader.h>

class ModelPart {
public:
    ModelPart(const QList<QVariant>& data, ModelPart* parent = nullptr);
    ~ModelPart();

    void appendChild(ModelPart* item);
    ModelPart* child(int row);
    int childCount() const;
    int columnCount() const;
    QVariant data(int column) const;
    void set( int column, const QVariant& value );
    ModelPart* parentItem();
    int row() const;

    void setColour(const unsigned char R, const unsigned char G, const unsigned char B);
    unsigned char getColourR();
    unsigned char getColourG();
    unsigned char getColourB();
void removeChild(int row);
    void setVisible(bool isVisible);
    bool visible();

    /** * 修改为接收 QString，这样在 MainWindow 中调用时无需 .toStdString()
     */
    void loadSTL(QString fileName);

    /** * 取消注释并内联实现，方便 MainWindow 获取 Actor
     */
    vtkSmartPointer<vtkActor> getActor() { return actor; }

    /** * VR 用的 Actor 获取（Exercise 2/5 可能会用到）
     */
    vtkActor* getNewActor();

private:
    QList<ModelPart*>                           m_childItems;
    QList<QVariant>                             m_itemData;
    ModelPart* m_parentItem;

    bool                                        isVisible;
    unsigned char m_r = 255;
    unsigned char m_g = 255;
    unsigned char m_b = 255;

    /* 取消注释这些变量，确保 loadSTL 有地方存储 VTK 对象 */
    vtkSmartPointer<vtkSTLReader>               file;
    vtkSmartPointer<vtkPolyDataMapper>          mapper; // 建议使用具体的 PolyDataMapper
    vtkSmartPointer<vtkActor>                   actor;
};

#endif
