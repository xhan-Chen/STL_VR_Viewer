
/**
 * @file ModelPart.cpp
 * @brief Implements ModelPart tree storage, STL loading, VTK filters, and actor styling.
 *
 * The implementation keeps the desktop VTK pipeline on the main thread and
 * creates deep-copied geometry snapshots for safe VR rendering.
 */

#include "ModelPart.h"
#include <QFileInfo>
#include <QLocale>
#include <QUuid>
#include <vtkSmartPointer.h>
#include <vtkPolyDataMapper.h>
#include <vtkSTLReader.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkShrinkPolyData.h>
#include <vtkClipPolyData.h>
#include <vtkPlane.h>
#include <vtkPolyData.h>

#include <algorithm>
#include <iterator>

ModelPart::ModelPart(const QList<QVariant>& data, ModelPart* parent)
    : m_itemData(data), m_parentItem(parent) {

    this->isVisible = true;
    m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    if (m_itemData.size() > 1) {
        m_itemData.replace(1, "Visible");
    }

    /* Initialize VTK components to null to prevent dangling pointer issues */
    file = nullptr;
    mapper = nullptr;
    actor = nullptr;

    /* Set default color to white */
    m_r = 255;
    m_g = 255;
    m_b = 255;
}

ModelPart::~ModelPart() {
    qDeleteAll(m_childItems);
}

void ModelPart::appendChild(ModelPart* item) {
    item->m_parentItem = this;
    m_childItems.append(item);
}

ModelPart* ModelPart::child(int row) {
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

void ModelPart::set(int column, const QVariant& value) {
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

    refreshAppearance();
}

unsigned char ModelPart::getColourR() { return m_r; }
unsigned char ModelPart::getColourG() { return m_g; }
unsigned char ModelPart::getColourB() { return m_b; }

void ModelPart::setVisible(bool isVisible) {
    this->isVisible = isVisible;
    if (m_itemData.size() > 1) {
        m_itemData.replace(1, isVisible ? "Visible" : "Hidden");
    }

    /* Synchronize visibility with the VTK actor */
    if (actor) {
        actor->SetVisibility(isVisible);
    }
    if (vrActor) {
        vrActor->SetVisibility(isVisible);
    }
}

bool ModelPart::visible() {
    return isVisible;
}

void ModelPart::loadSTL(QString fileName)
{
    QFileInfo fileInfo(fileName);
    m_filePath = fileInfo.absoluteFilePath();
    m_triangleCount = 0;
    std::fill(std::begin(m_bounds), std::end(m_bounds), 0.0);

    file = vtkSmartPointer<vtkSTLReader>::New();
    file->SetFileName(fileName.toStdString().c_str());
    file->MergingOff();
    file->Update();
    originalDataPort = file->GetOutputPort();

    vtkPolyData* polyData = file->GetOutput();
    if (polyData) {
        m_triangleCount = polyData->GetNumberOfPolys();
        polyData->GetBounds(m_bounds);
    }

    mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->ScalarVisibilityOff();
    mapper->StaticOff();

    /* Initialize filter components */
    shrinkFilter = vtkSmartPointer<vtkShrinkPolyData>::New();
    shrinkFilter->SetShrinkFactor(0.8);

    clipPlane = vtkSmartPointer<vtkPlane>::New();
    clipPlane->SetOrigin(
        (m_bounds[0] + m_bounds[1]) * 0.5,
        (m_bounds[2] + m_bounds[3]) * 0.5,
        (m_bounds[4] + m_bounds[5]) * 0.5);
    clipPlane->SetNormal(1.0, 0.0, 0.0);

    clipFilter = vtkSmartPointer<vtkClipPolyData>::New();
    clipFilter->SetClipFunction(clipPlane);

    actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);

    /* Initialize actor properties */
    actor->GetProperty()->SetColor(m_r / 255.0, m_g / 255.0, m_b / 255.0);
    actor->GetProperty()->SetInterpolationToPhong();
    actor->GetProperty()->SetAmbient(0.18);
    actor->GetProperty()->SetDiffuse(0.72);
    actor->GetProperty()->SetSpecular(0.28);
    actor->GetProperty()->SetSpecularPower(32.0);
    actor->SetVisibility(this->isVisible);
    applyTransformToActors();
    refreshAppearance();

    updatePipeline();
}

vtkActor* ModelPart::getNewActor() {
    if (!file) return nullptr;

    vtkSmartPointer<vtkPolyData> vrData = createGeometrySnapshot();
    if (!vrData || vrData->GetNumberOfPoints() == 0 || vrData->GetNumberOfCells() == 0) {
        return nullptr;
    }

    vtkSmartPointer<vtkPolyDataMapper> vrMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    vrMapper->SetInputData(vrData);
    vrMapper->ScalarVisibilityOff();
    vrMapper->StaticOn();

    /* Create actor instance for VR environment */
    vtkSmartPointer<vtkActor> newVrActor = vtkSmartPointer<vtkActor>::New();
    newVrActor->SetMapper(vrMapper);

    /* Synchronize properties, visibility, and scale with existing actor */
    if (this->actor) {
        vtkSmartPointer<vtkProperty> vrProperty = vtkSmartPointer<vtkProperty>::New();
        vrProperty->DeepCopy(this->actor->GetProperty());
        newVrActor->SetProperty(vrProperty);
        newVrActor->SetVisibility(this->actor->GetVisibility());
    }

    this->vrActor = newVrActor;
    applyTransformToActors();
    refreshAppearance();
    return this->vrActor;
}

vtkSmartPointer<vtkPolyData> ModelPart::createGeometrySnapshot()
{
    vtkSmartPointer<vtkPolyData> copy = vtkSmartPointer<vtkPolyData>::New();
    if (!file) {
        return copy;
    }

    updatePipeline();

    vtkPolyData* source = nullptr;
    if (mapper) {
        mapper->Update();
        source = mapper->GetInput();
    }
    if (!source || source->GetNumberOfPoints() == 0 || source->GetNumberOfCells() == 0) {
        file->Update();
        source = file->GetOutput();
    }

    if (source) {
        copy->DeepCopy(source);
    }

    return copy;
}

void ModelPart::detachVrActor()
{
    vrActor = nullptr;
}

void ModelPart::removeChild(int row) {
    if (row >= 0 && row < m_childItems.size()) {
        /* Take pointer from list and delete memory recursively */
        ModelPart* item = m_childItems.takeAt(row);
        delete item;
    }
}

void ModelPart::setShrinkFilterActive(bool active) {
    if (!this->mapper || !this->originalDataPort) return;

    this->shrinkActive = active;

    /* Toggle connection between direct port and filter output */
    if (active) {
        this->mapper->SetInputConnection(this->shrinkFilter->GetOutputPort());
    }
    else {
        this->mapper->SetInputConnection(this->originalDataPort);
    }
}

void ModelPart::setShrinkEnabled(bool enabled) { shrinkEnabled = enabled; }

void ModelPart::setClipEnabled(bool enabled) { clipEnabled = enabled; }

void ModelPart::setFilterOrder(int order) { filterOrder = order; }

bool ModelPart::getShrinkEnabled() const { return shrinkEnabled; }

bool ModelPart::getClipEnabled() const { return clipEnabled; }

int ModelPart::getFilterOrder() const { return filterOrder; }

void ModelPart::updatePipeline()
{
    if (!file || !mapper) return;

    mapper->StaticOff();

    /* Update pipeline based on active filter combination */
    if (!shrinkEnabled && !clipEnabled) {
        mapper->SetInputConnection(file->GetOutputPort());
    }
    else if (shrinkEnabled && !clipEnabled) {
        shrinkFilter->SetInputConnection(file->GetOutputPort());
        mapper->SetInputConnection(shrinkFilter->GetOutputPort());
    }
    else if (!shrinkEnabled && clipEnabled) {
        clipFilter->SetInputConnection(file->GetOutputPort());
        mapper->SetInputConnection(clipFilter->GetOutputPort());
    }
    else {
        /* Handle nested filter execution order */
        if (filterOrder == 0) {
            shrinkFilter->SetInputConnection(file->GetOutputPort());
            clipFilter->SetInputConnection(shrinkFilter->GetOutputPort());
            mapper->SetInputConnection(clipFilter->GetOutputPort());
        }
        else {
            clipFilter->SetInputConnection(file->GetOutputPort());
            shrinkFilter->SetInputConnection(clipFilter->GetOutputPort());
            mapper->SetInputConnection(shrinkFilter->GetOutputPort());
        }
    }

    mapper->Modified();
    mapper->Update();
    vtkPolyData* mappedData = mapper->GetInput();
    if (mappedData && (mappedData->GetNumberOfPoints() == 0 || mappedData->GetNumberOfCells() == 0)) {
        mapper->SetInputConnection(file->GetOutputPort());
        mapper->Modified();
        mapper->Update();
    }

    if (actor) {
        actor->Modified();
    }
}

void ModelPart::setScale(double s) {
    m_scale = s;
    applyTransformToActors();
}

void ModelPart::setPosition(double x, double y, double z) {
    m_position[0] = x;
    m_position[1] = y;
    m_position[2] = z;
    applyTransformToActors();
}

void ModelPart::getPosition(double& x, double& y, double& z) const {
    x = m_position[0];
    y = m_position[1];
    z = m_position[2];
}

void ModelPart::setRotation(double x, double y, double z) {
    m_rotation[0] = x;
    m_rotation[1] = y;
    m_rotation[2] = z;
    applyTransformToActors();
}

void ModelPart::getRotation(double& x, double& y, double& z) const {
    x = m_rotation[0];
    y = m_rotation[1];
    z = m_rotation[2];
}

void ModelPart::setTransformState(const TransformState& state) {
    m_position[0] = state.position[0];
    m_position[1] = state.position[1];
    m_position[2] = state.position[2];
    m_rotation[0] = state.rotation[0];
    m_rotation[1] = state.rotation[1];
    m_rotation[2] = state.rotation[2];
    m_scale = state.scale;
    applyTransformToActors();
}

ModelPart::TransformState ModelPart::transformState() const {
    TransformState state;
    state.position = { m_position[0], m_position[1], m_position[2] };
    state.rotation = { m_rotation[0], m_rotation[1], m_rotation[2] };
    state.scale = m_scale;
    return state;
}

void ModelPart::setHighlighted(bool highlighted) {
    m_highlighted = highlighted;
    refreshAppearance();
}

void ModelPart::setSelectedInView(bool selected) {
    m_selectedInView = selected;
    refreshAppearance();
}

void ModelPart::setGlowEnabled(bool enabled) {
    m_glowEnabled = enabled;
    refreshAppearance();
}

void ModelPart::setGlowColour(const unsigned char R, const unsigned char G, const unsigned char B) {
    m_glowR = R;
    m_glowG = G;
    m_glowB = B;
    refreshAppearance();
}

void ModelPart::applyTransformToActors() {
    auto apply = [this](vtkActor* target) {
        if (!target) return;
        target->SetPosition(m_position);
        target->SetOrientation(m_rotation);
        target->SetScale(m_scale, m_scale, m_scale);
        target->Modified();
    };

    apply(actor);
    apply(vrActor);
}

void ModelPart::refreshAppearance() {
    auto apply = [this](vtkActor* target) {
        if (!target) return;

        vtkProperty* property = target->GetProperty();
        const double baseR = m_r / 255.0;
        const double baseG = m_g / 255.0;
        const double baseB = m_b / 255.0;

        property->SetColor(baseR, baseG, baseB);
        property->SetDiffuse(0.72);
        property->SetSpecular(0.28);
        property->SetSpecularPower(32.0);
        property->SetAmbient(0.18);
        property->EdgeVisibilityOff();
        property->SetLineWidth(1.0);
        property->SetEdgeWidth(1.0);
        property->SetEmissiveFactor(0.0, 0.0, 0.0);

        if (m_glowEnabled) {
            const double glowR = m_glowR / 255.0;
            const double glowG = m_glowG / 255.0;
            const double glowB = m_glowB / 255.0;
            property->SetAmbient(0.85);
            property->SetDiffuse(0.45);
            property->SetSpecular(0.75);
            property->SetSpecularPower(90.0);
            property->SetEmissiveFactor(glowR, glowG, glowB);
            property->SetEdgeColor(glowR, glowG, glowB);
            property->SetEdgeVisibility(true);
            property->SetEdgeWidth(2.0);
        }

        if (m_highlighted || m_selectedInView) {
            const double highlightR = m_selectedInView ? 0.15 : 1.0;
            const double highlightG = m_selectedInView ? 0.62 : 0.78;
            const double highlightB = m_selectedInView ? 1.0 : 0.25;
            property->SetEdgeColor(highlightR, highlightG, highlightB);
            property->SetEdgeVisibility(true);
            property->SetLineWidth(m_selectedInView ? 4.0 : 3.0);
            property->SetEdgeWidth(m_selectedInView ? 4.0 : 3.0);
        }

        target->Modified();
    };

    apply(actor);
    apply(vrActor);
}

void ModelPart::setGroupName(const QString& name) {
    m_groupName = name;
}

QString ModelPart::getGroupName() const {
    return m_groupName;
}

bool ModelPart::isLockedInGroup() const {
    return !m_groupName.isEmpty();
}

QString ModelPart::summary() const
{
    if (!hasGeometry()) {
        return QStringLiteral("No STL loaded");
    }

    const QLocale locale;
    return QStringLiteral("%1 | %2 triangles | Bounds: %3 x %4 x %5")
        .arg(m_filePath)
        .arg(locale.toString(m_triangleCount))
        .arg(locale.toString(m_bounds[1] - m_bounds[0], 'f', 2))
        .arg(locale.toString(m_bounds[3] - m_bounds[2], 'f', 2))
        .arg(locale.toString(m_bounds[5] - m_bounds[4], 'f', 2));
}
