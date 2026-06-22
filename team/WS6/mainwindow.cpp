/**
 * @file mainwindow.cpp
 * @brief Implements the Qt main window, desktop VTK renderer, model loading, and VR coordination.
 *
 * This file contains the application-level workflow: loading STL files, building
 * the tree model, rendering the desktop scene, managing HDR environments, and
 * passing safe actor snapshots to the VR rendering thread.
 */

#include <vtkNew.h>
#include <vtkCylinderSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkCamera.h>
#include <vtkSTLReader.h>
#include <vtkLight.h>
#include <vtkLightCollection.h>
#include <vtkMath.h>
#include <vtkInteractorStyleTrackballActor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkShrinkPolyData.h>
#include <vtkAxesActor.h>
#include <vtkCallbackCommand.h>
#include <vtkCellArray.h>
#include <vtkHDRReader.h>
#include <vtkImageData.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPropPicker.h>
#include <vtkSkybox.h>
#include <vtkTexturedSphereSource.h>
#include <vtkTexture.h>
#include "mainwindow.h"
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include "VRRenderThread.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QPushButton>
#include "QFileDialog.h"
#include "dialog.h"
#include <QDebug>
#include <QAbstractItemModel>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFileInfoList>
#include <QFormLayout>
#include <QFont>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>
#include <QProgressBar>
#include <QProgressDialog>
#include <QScrollBar>
#include <QSettings>
#include <QSet>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStyle>
#include <QStatusBar>
#include <QSlider>
#include <QToolBar>
#include <QTimer>
#include <QUndoCommand>
#include <QUndoStack>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

namespace {
/**
 * @brief Built-in correction that rotates HDR panoramas upright before user tilt is applied.
 */
constexpr double kHdrUprightRollDegrees = -90.0;

/**
 * @enum ToolbarIcon
 * @brief Identifiers for the custom painter-generated toolbar icons.
 */
enum class ToolbarIcon {
    OpenFile,    ///< Open one STL file.
    OpenFolder,  ///< Open a folder of STL files.
    Add,         ///< Add a tree item.
    Delete,      ///< Delete a tree item.
    Options,     ///< Open part options.
    Group,       ///< Group selected parts.
    Play,        ///< Start VR.
    Stop,        ///< Stop VR.
    Pause,       ///< Pause VR.
    Filter,      ///< Toggle visibility filtering.
    Fit,         ///< Fit camera to model.
    Undo,        ///< Undo last change.
    Redo,        ///< Redo last undone change.
    Grid,        ///< Toggle floor grid.
    Performance, ///< Toggle performance mode.
    Showcase     ///< Toggle showcase spin.
};

/**
 * @brief Creates a small custom toolbar icon using QPainter.
 * @param type Icon shape to draw.
 * @param accent Accent colour used for the icon stroke.
 * @return Ready-to-use QIcon.
 */
QIcon makeToolbarIcon(ToolbarIcon type, QColor accent = QColor(56, 189, 248))
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(71, 85, 105), 1.0));
    painter.setBrush(QColor(15, 23, 42));
    painter.drawRoundedRect(QRectF(2.5, 2.5, 27.0, 27.0), 7.0, 7.0);

    QPen pen(accent, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    switch (type) {
    case ToolbarIcon::OpenFile:
        painter.drawRoundedRect(QRectF(10, 7, 12, 17), 1.5, 1.5);
        painter.drawLine(QPointF(14, 15), QPointF(22, 15));
        painter.drawLine(QPointF(18, 11), QPointF(22, 15));
        painter.drawLine(QPointF(18, 19), QPointF(22, 15));
        break;
    case ToolbarIcon::OpenFolder:
        painter.drawPolyline(QPolygonF{
            QPointF(7, 12), QPointF(12, 12), QPointF(14, 9), QPointF(22, 9),
            QPointF(25, 13), QPointF(25, 23), QPointF(7, 23), QPointF(7, 12)
        });
        break;
    case ToolbarIcon::Add:
        painter.drawLine(QPointF(16, 9), QPointF(16, 23));
        painter.drawLine(QPointF(9, 16), QPointF(23, 16));
        break;
    case ToolbarIcon::Delete:
        painter.drawLine(QPointF(11, 11), QPointF(21, 11));
        painter.drawLine(QPointF(13, 11), QPointF(14, 24));
        painter.drawLine(QPointF(19, 11), QPointF(18, 24));
        painter.drawLine(QPointF(12, 24), QPointF(20, 24));
        painter.drawLine(QPointF(14, 8), QPointF(18, 8));
        break;
    case ToolbarIcon::Options:
        painter.drawLine(QPointF(9, 11), QPointF(23, 11));
        painter.drawEllipse(QPointF(14, 11), 2.0, 2.0);
        painter.drawLine(QPointF(9, 16), QPointF(23, 16));
        painter.drawEllipse(QPointF(19, 16), 2.0, 2.0);
        painter.drawLine(QPointF(9, 21), QPointF(23, 21));
        painter.drawEllipse(QPointF(12, 21), 2.0, 2.0);
        break;
    case ToolbarIcon::Group:
        painter.drawRoundedRect(QRectF(8, 9, 8, 7), 2, 2);
        painter.drawRoundedRect(QRectF(16, 16, 8, 7), 2, 2);
        painter.drawLine(QPointF(14, 16), QPointF(18, 16));
        break;
    case ToolbarIcon::Play:
        painter.setBrush(accent);
        painter.drawPolygon(QPolygonF{ QPointF(12, 9), QPointF(23, 16), QPointF(12, 23) });
        break;
    case ToolbarIcon::Stop:
        painter.setBrush(accent);
        painter.drawRoundedRect(QRectF(11, 11, 10, 10), 2, 2);
        break;
    case ToolbarIcon::Pause:
        painter.setBrush(accent);
        painter.drawRoundedRect(QRectF(11, 10, 4, 12), 1.5, 1.5);
        painter.drawRoundedRect(QRectF(18, 10, 4, 12), 1.5, 1.5);
        break;
    case ToolbarIcon::Filter:
        painter.drawPolyline(QPolygonF{
            QPointF(9, 10), QPointF(23, 10), QPointF(18, 16),
            QPointF(18, 23), QPointF(14, 21), QPointF(14, 16), QPointF(9, 10)
        });
        break;
    case ToolbarIcon::Fit:
        painter.drawLine(QPointF(9, 13), QPointF(9, 9));
        painter.drawLine(QPointF(9, 9), QPointF(13, 9));
        painter.drawLine(QPointF(23, 13), QPointF(23, 9));
        painter.drawLine(QPointF(23, 9), QPointF(19, 9));
        painter.drawLine(QPointF(9, 19), QPointF(9, 23));
        painter.drawLine(QPointF(9, 23), QPointF(13, 23));
        painter.drawLine(QPointF(23, 19), QPointF(23, 23));
        painter.drawLine(QPointF(23, 23), QPointF(19, 23));
        break;
    case ToolbarIcon::Undo:
        painter.drawArc(QRectF(9, 10, 15, 13), 20 * 16, 250 * 16);
        painter.drawLine(QPointF(10, 16), QPointF(8, 10));
        painter.drawLine(QPointF(10, 16), QPointF(15, 14));
        break;
    case ToolbarIcon::Redo:
        painter.drawArc(QRectF(8, 10, 15, 13), -90 * 16, 250 * 16);
        painter.drawLine(QPointF(22, 16), QPointF(24, 10));
        painter.drawLine(QPointF(22, 16), QPointF(17, 14));
        break;
    case ToolbarIcon::Grid:
        for (int i = 0; i < 3; ++i) {
            const double offset = 10.0 + i * 6.0;
            painter.drawLine(QPointF(8, offset), QPointF(24, offset));
            painter.drawLine(QPointF(offset, 8), QPointF(offset, 24));
        }
        break;
    case ToolbarIcon::Performance:
        painter.setBrush(accent);
        painter.drawPolygon(QPolygonF{
            QPointF(17, 7), QPointF(10, 18), QPointF(15, 18),
            QPointF(13, 25), QPointF(22, 13), QPointF(17, 13)
        });
        break;
    case ToolbarIcon::Showcase:
        painter.drawArc(QRectF(8, 8, 16, 16), 40 * 16, 280 * 16);
        painter.drawLine(QPointF(21, 9), QPointF(24, 9));
        painter.drawLine(QPointF(21, 9), QPointF(22, 13));
        painter.drawEllipse(QPointF(16, 16), 2.2, 2.2);
        break;
    }

    return QIcon(pixmap);
}

vtkSmartPointer<vtkImageData> toneMapHdrForTexture(vtkImageData* hdrImage)
{
    vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
    if (!hdrImage) return image;

    int dims[3] = { 0, 0, 0 };
    hdrImage->GetDimensions(dims);
    image->SetDimensions(dims);
    image->SetOrigin(hdrImage->GetOrigin());
    image->SetSpacing(hdrImage->GetSpacing());
    image->AllocateScalars(VTK_UNSIGNED_CHAR, 3);

    const int components = std::max(1, hdrImage->GetNumberOfScalarComponents());
    constexpr double exposure = 0.75;
    constexpr double gamma = 1.0 / 2.2;

    for (int z = 0; z < std::max(1, dims[2]); ++z) {
        for (int y = 0; y < dims[1]; ++y) {
            for (int x = 0; x < dims[0]; ++x) {
                auto* output = static_cast<unsigned char*>(image->GetScalarPointer(x, y, z));
                for (int c = 0; c < 3; ++c) {
                    const int sourceComponent = std::min(c, components - 1);
                    const double source = std::max(0.0, hdrImage->GetScalarComponentAsDouble(x, y, z, sourceComponent));
                    const double mapped = std::pow(std::clamp(1.0 - std::exp(-source * exposure), 0.0, 1.0), gamma);
                    output[c] = static_cast<unsigned char>(std::lround(mapped * 255.0));
                }
            }
        }
    }

    return image;
}

double wrapUnit(double value)
{
    value -= std::floor(value);
    return value < 0.0 ? value + 1.0 : value;
}

void sampleEnvironmentPixel(vtkImageData* image, double u, double v, unsigned char rgb[3])
{
    int dims[3] = { 0, 0, 0 };
    image->GetDimensions(dims);
    const int width = std::max(1, dims[0]);
    const int height = std::max(1, dims[1]);

    u = wrapUnit(u);
    v = std::clamp(v, 0.0, 1.0);

    const double sx = u * width - 0.5;
    const double sy = v * height - 0.5;
    const int x0 = static_cast<int>(std::floor(sx));
    const int y0 = std::clamp(static_cast<int>(std::floor(sy)), 0, height - 1);
    const int x1 = x0 + 1;
    const int y1 = std::clamp(y0 + 1, 0, height - 1);
    const double tx = sx - std::floor(sx);
    const double ty = sy - std::floor(sy);
    const int wx0 = (x0 % width + width) % width;
    const int wx1 = (x1 % width + width) % width;

    auto* p00 = static_cast<unsigned char*>(image->GetScalarPointer(wx0, y0, 0));
    auto* p10 = static_cast<unsigned char*>(image->GetScalarPointer(wx1, y0, 0));
    auto* p01 = static_cast<unsigned char*>(image->GetScalarPointer(wx0, y1, 0));
    auto* p11 = static_cast<unsigned char*>(image->GetScalarPointer(wx1, y1, 0));

    for (int c = 0; c < 3; ++c) {
        const double top = p00[c] * (1.0 - tx) + p10[c] * tx;
        const double bottom = p01[c] * (1.0 - tx) + p11[c] * tx;
        rgb[c] = static_cast<unsigned char>(std::lround(std::clamp(top * (1.0 - ty) + bottom * ty, 0.0, 255.0)));
    }
}

vtkSmartPointer<vtkImageData> orientedEnvironmentImage(vtkImageData* sourceImage,
                                                       double rollDegrees,
                                                       double pitchDegrees,
                                                       double headingDegrees)
{
    vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
    if (!sourceImage) return image;

    int dims[3] = { 0, 0, 0 };
    sourceImage->GetDimensions(dims);
    image->SetDimensions(dims);
    image->SetOrigin(sourceImage->GetOrigin());
    image->SetSpacing(sourceImage->GetSpacing());
    image->AllocateScalars(VTK_UNSIGNED_CHAR, 3);

    const int width = std::max(1, dims[0]);
    const int height = std::max(1, dims[1]);
    constexpr double pi = 3.14159265358979323846;
    const double inverseRoll = vtkMath::RadiansFromDegrees(-rollDegrees);
    const double inversePitch = vtkMath::RadiansFromDegrees(-pitchDegrees);
    const double inverseHeading = vtkMath::RadiansFromDegrees(-headingDegrees);
    const double cosRoll = std::cos(inverseRoll);
    const double sinRoll = std::sin(inverseRoll);
    const double cosPitch = std::cos(inversePitch);
    const double sinPitch = std::sin(inversePitch);
    const double cosHeading = std::cos(inverseHeading);
    const double sinHeading = std::sin(inverseHeading);

    for (int y = 0; y < height; ++y) {
        const double v = (y + 0.5) / height;
        const double latitude = (0.5 - v) * pi;
        const double cosLatitude = std::cos(latitude);
        for (int x = 0; x < width; ++x) {
            const double u = (x + 0.5) / width;
            const double longitude = (u - 0.5) * 2.0 * pi;

            double dx = cosLatitude * std::sin(longitude);
            double dy = std::sin(latitude);
            double dz = cosLatitude * std::cos(longitude);

            const double rolledX = cosRoll * dx - sinRoll * dy;
            const double rolledY = sinRoll * dx + cosRoll * dy;
            const double rolledZ = dz;

            const double pitchedX = rolledX;
            const double pitchedY = cosPitch * rolledY - sinPitch * rolledZ;
            const double pitchedZ = sinPitch * rolledY + cosPitch * rolledZ;

            const double sourceX = cosHeading * pitchedX + sinHeading * pitchedZ;
            const double sourceY = pitchedY;
            const double sourceZ = -sinHeading * pitchedX + cosHeading * pitchedZ;

            const double sourceLongitude = std::atan2(sourceX, sourceZ);
            const double sourceLatitude = std::asin(std::clamp(sourceY, -1.0, 1.0));
            const double sourceU = sourceLongitude / (2.0 * pi) + 0.5;
            const double sourceV = 0.5 - sourceLatitude / pi;

            unsigned char rgb[3] = { 0, 0, 0 };
            sampleEnvironmentPixel(sourceImage, sourceU, sourceV, rgb);
            auto* output = static_cast<unsigned char*>(image->GetScalarPointer(x, y, 0));
            output[0] = rgb[0];
            output[1] = rgb[1];
            output[2] = rgb[2];
        }
    }

    return image;
}

/**
 * @class LambdaUndoCommand
 * @brief Small QUndoCommand wrapper that stores undo and redo lambdas.
 *
 * This avoids creating many one-off command subclasses for simple transform
 * operations while still integrating with QUndoStack.
 */
class LambdaUndoCommand : public QUndoCommand
{
public:
    /**
     * @brief Creates a command from callable undo and redo operations.
     * @param label Text shown in the undo stack.
     * @param undoFn Function executed when the command is undone.
     * @param redoFn Function executed when the command is redone.
     */
    LambdaUndoCommand(QString label, std::function<void()> undoFn, std::function<void()> redoFn)
        : QUndoCommand(std::move(label)), undoAction(std::move(undoFn)), redoAction(std::move(redoFn)) {}

    /**
     * @brief Executes the stored undo operation.
     */
    void undo() override { if (undoAction) undoAction(); }

    /**
     * @brief Executes the stored redo operation.
     */
    void redo() override { if (redoAction) redoAction(); }

private:
    std::function<void()> undoAction; ///< Callable used when the user performs undo.
    std::function<void()> redoAction; ///< Callable used when the user performs redo.
};
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle(tr("VR Model Viewer - STL Assembly Studio"));
    resize(1480, 860);

    this->partList = new ModelPartList("PartsList", this);
    undoStack = new QUndoStack(this);
    ui->treeView->setModel(this->partList);

    setupProfessionalUi();
    configureActions();
    setupRenderingPipeline();

    connect(ui->pushButton, &QPushButton::released, this, &MainWindow::handleButton);
    connect(ui->pushButton_2, &QPushButton::released, this, &MainWindow::on_actionItemOptions_triggered);
    connect(this, &MainWindow::statusUpdateMessage, ui->statusBar, &QStatusBar::showMessage);
    connect(ui->treeView, &QTreeView::clicked, this, &MainWindow::handleTreeClicked);
    connect(searchEdit, &QLineEdit::textChanged, this, &MainWindow::handleSearchTextChanged);
    connect(partList, &QAbstractItemModel::dataChanged, this,
        [this](const QModelIndex&, const QModelIndex&, const QList<int>& roles) {
        if (bulkLoading) return;

        const bool visualOnly = !roles.isEmpty()
            && std::all_of(roles.cbegin(), roles.cend(), [](int role) {
                return role == Qt::BackgroundRole;
            });

        if (!visualOnly) {
            updateRender();
            updateSceneSummary();
        }
        });
    connect(partList, &QAbstractItemModel::rowsInserted, this,
        [this](const QModelIndex&, int, int) { if (!bulkLoading) updateSceneSummary(); });
    connect(partList, &QAbstractItemModel::rowsRemoved, this,
        [this](const QModelIndex&, int, int) { if (!bulkLoading) updateSceneSummary(); });
    connect(ui->groupTreeWidget->model(), &QAbstractItemModel::rowsInserted, this, [this]() {
        QTimer::singleShot(0, this, &MainWindow::cleanupEmptyGroups);
        });
    connect(ui->groupTreeWidget->model(), &QAbstractItemModel::rowsRemoved, this, [this]() {
        QTimer::singleShot(0, this, &MainWindow::cleanupEmptyGroups);
        });

    ui->actionPause_VR->setVisible(false);
    ui->actionStop_VR->setVisible(false);

    updateSceneSummary();
    syncTransformControls(nullptr);
    emit statusUpdateMessage("Ready. Open a folder to load the complete STL assembly.", 5000);
}

MainWindow::~MainWindow()
{
    stopVRThread();
    delete ui;
}

void MainWindow::setupProfessionalUi()
{
    ui->zoomSlider->setRange(25, 300);
    ui->zoomSlider->setValue(100);
    ui->zoomSlider->setToolTip(tr("Camera zoom"));

    ui->itemScaleSlider->setRange(10, 300);
    ui->itemScaleSlider->setValue(100);
    ui->itemScaleSlider->setToolTip(tr("Selected part or group scale"));

    treeScaleSlider = new QSlider(Qt::Horizontal, this);
    treeScaleSlider->setRange(80, 160);
    treeScaleSlider->setValue(100);
    treeScaleSlider->setToolTip(tr("Scale the part tree text and spacing"));

    ui->lightSlider->setRange(0, 200);
    ui->lightSlider->setValue(100);
    ui->lightSlider->setToolTip(tr("Key light brightness"));

    ui->pushButton->setText(tr("Add Child"));
    ui->pushButton_2->setText(tr("Options"));

    searchEdit = new QLineEdit(this);
    searchEdit->setClearButtonEnabled(true);
    searchEdit->setPlaceholderText(tr("Search loaded parts"));

    hdrComboBox = new QComboBox(this);
    hdrComboBox->setToolTip(tr("Choose a studio HDR environment or the default gradient background"));
    importHdrButton = new QPushButton(tr("Add HDR"), this);
    importHdrButton->setToolTip(tr("Import a custom HDR environment file"));

    vrViewModeCombo = new QComboBox(this);
    vrViewModeCombo->addItem(tr("Standing View"), VRRenderThread::STANDING_VIEW);
    vrViewModeCombo->addItem(tr("Sitting View"), VRRenderThread::SITTING_VIEW);
    vrViewModeCombo->setToolTip(tr("Choose the VR starting height and movement mode"));
    vrViewAngleCombo = new QComboBox(this);
    vrViewAngleCombo->addItem(tr("Front 3/4"), 28.0);
    vrViewAngleCombo->addItem(tr("Front"), 0.0);
    vrViewAngleCombo->addItem(tr("Left Side"), 90.0);
    vrViewAngleCombo->addItem(tr("Right Side"), -90.0);
    vrViewAngleCombo->addItem(tr("Rear"), 180.0);
    vrViewAngleCombo->setToolTip(tr("Choose the VR reset angle around the car"));
    resetVrViewButton = new QPushButton(tr("Reset VR View"), this);
    resetVrViewButton->setToolTip(tr("Return the VR headset view to a clear position in front of the car"));

    auto createSpin = [this](double minimum, double maximum, double step, int decimals) {
        auto* spin = new QDoubleSpinBox(this);
        spin->setRange(minimum, maximum);
        spin->setSingleStep(step);
        spin->setDecimals(decimals);
        spin->setKeyboardTracking(false);
        spin->setButtonSymbols(QAbstractSpinBox::PlusMinus);
        spin->setMinimumWidth(54);
        spin->setMaximumWidth(86);
        return spin;
    };

    for (int i = 0; i < 3; ++i) {
        positionSpin[i] = createSpin(-100000.0, 100000.0, 1.0, 3);
        rotationSpin[i] = createSpin(-360.0, 360.0, 5.0, 2);
    }
    hdrTiltSpin = createSpin(-180.0, 180.0, 15.0, 1);
    hdrTiltSpin->setValue(25.0);
    hdrTiltSpin->setSuffix(tr(" deg"));
    hdrTiltSpin->setToolTip(tr("Fine-tune the HDR tilt around the X direction"));
    hdrTiltYSpin = createSpin(-180.0, 180.0, 15.0, 1);
    hdrTiltYSpin->setValue(65.0);
    hdrTiltYSpin->setSuffix(tr(" deg"));
    hdrTiltYSpin->setToolTip(tr("Fine-tune the HDR tilt around the Y direction"));
    hdrHeadingSpin = createSpin(-180.0, 180.0, 15.0, 1);
    hdrHeadingSpin->setValue(-10.0);
    hdrHeadingSpin->setSuffix(tr(" deg"));
    hdrHeadingSpin->setToolTip(tr("Rotate the HDR around the car"));
    vrModelSizeSpin = createSpin(0.25, 8.0, 0.1, 2);
    vrModelSizeSpin->setValue(2.4);
    vrModelSizeSpin->setSuffix(tr(" m"));
    vrModelSizeSpin->setToolTip(tr("Approximate car size in VR"));
    vrDistanceSpin = createSpin(0.4, 8.0, 0.1, 2);
    vrDistanceSpin->setValue(1.1);
    vrDistanceSpin->setSuffix(tr(" m"));
    vrDistanceSpin->setToolTip(tr("Viewing distance from the car"));
    vrHeightOffsetSpin = createSpin(-2.0, 2.0, 0.05, 2);
    vrHeightOffsetSpin->setValue(0.0);
    vrHeightOffsetSpin->setSuffix(tr(" m"));
    vrHeightOffsetSpin->setToolTip(tr("Fine-tune VR eye height"));
    transformScaleSpin = createSpin(0.01, 100.0, 0.05, 3);
    transformScaleSpin->setValue(1.0);
    applyTransformButton = new QPushButton(tr("Apply Transform"), this);
    resetTransformButton = new QPushButton(tr("Reset"), this);

    sceneStatsLabel = new QLabel(this);
    selectionDetailsLabel = new QLabel(tr("No part selected"), this);
    selectionDetailsLabel->setMinimumWidth(360);
    loadProgressBar = new QProgressBar(this);
    loadProgressBar->setRange(0, 100);
    loadProgressBar->setMaximumWidth(180);
    loadProgressBar->setVisible(false);

    vrTransformCommitTimer = new QTimer(this);
    vrTransformCommitTimer->setSingleShot(true);

    ui->statusBar->addPermanentWidget(selectionDetailsLabel, 1);
    ui->statusBar->addPermanentWidget(sceneStatsLabel);
    ui->statusBar->addPermanentWidget(loadProgressBar);

    ui->treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->treeView->setMinimumWidth(0);
    ui->treeView->setAlternatingRowColors(true);
    ui->treeView->setUniformRowHeights(true);
    ui->treeView->setAnimated(false);
    ui->treeView->setEditTriggers(
        QAbstractItemView::DoubleClicked |
        QAbstractItemView::EditKeyPressed |
        QAbstractItemView::AnyKeyPressed);
    ui->treeView->setContextMenuPolicy(Qt::ActionsContextMenu);
    ui->treeView->header()->setStretchLastSection(false);
    ui->treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->treeView->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    ui->groupTreeWidget->setHeaderLabel(tr("Locked Groups"));
    ui->groupTreeWidget->setMinimumWidth(0);
    ui->groupTreeWidget->setAlternatingRowColors(true);
    ui->groupTreeWidget->setContextMenuPolicy(Qt::ActionsContextMenu);

    auto* central = new QWidget(this);
    auto* rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(8);

    mainSplitter = new QSplitter(Qt::Horizontal, central);
    mainSplitter->setChildrenCollapsible(true);
    mainSplitter->setHandleWidth(8);
    rootLayout->addWidget(mainSplitter);

    auto* leftPanel = new QWidget(mainSplitter);
    leftPanel->setMinimumWidth(0);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    auto* partTitle = new QLabel(tr("Model Parts"), leftPanel);
    partTitle->setObjectName("panelTitle");
    leftLayout->addWidget(partTitle);
    leftLayout->addWidget(searchEdit);
    leftLayout->addWidget(ui->treeView, 1);

    auto* controls = new QGroupBox(tr("Viewer Controls"), leftPanel);
    controls->setMinimumWidth(0);
    auto* controlsLayout = new QFormLayout(controls);
    controlsLayout->setLabelAlignment(Qt::AlignLeft);
    controlsLayout->addRow(tr("Zoom"), ui->zoomSlider);
    controlsLayout->addRow(tr("Part Scale"), ui->itemScaleSlider);
    controlsLayout->addRow(tr("Tree Size"), treeScaleSlider);
    controlsLayout->addRow(tr("Light"), ui->lightSlider);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addWidget(ui->pushButton);
    buttonRow->addWidget(ui->pushButton_2);
    controlsLayout->addRow(buttonRow);
    leftLayout->addWidget(controls);

    auto* transformControls = new QGroupBox(tr("Transform"), leftPanel);
    transformControls->setMinimumWidth(0);
    auto* transformLayout = new QFormLayout(transformControls);
    auto* positionRow = new QHBoxLayout();
    auto* rotationRow = new QHBoxLayout();
    for (int i = 0; i < 3; ++i) {
        positionRow->addWidget(positionSpin[i]);
        rotationRow->addWidget(rotationSpin[i]);
    }
    auto* transformButtonRow = new QHBoxLayout();
    transformButtonRow->addWidget(applyTransformButton);
    transformButtonRow->addWidget(resetTransformButton);
    transformLayout->addRow(tr("Position XYZ"), positionRow);
    transformLayout->addRow(tr("Rotation XYZ"), rotationRow);
    transformLayout->addRow(tr("Scale"), transformScaleSpin);
    transformLayout->addRow(transformButtonRow);
    leftLayout->addWidget(transformControls);

    auto* centerPanel = new QWidget(mainSplitter);
    centerPanel->setMinimumWidth(420);
    auto* centerLayout = new QVBoxLayout(centerPanel);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->addWidget(ui->vtkWidget, 1);

    auto* rightPanel = new QWidget(mainSplitter);
    rightPanel->setMinimumWidth(0);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);
    auto* groupTitle = new QLabel(tr("Groups"), rightPanel);
    groupTitle->setObjectName("panelTitle");
    rightLayout->addWidget(groupTitle);
    auto* environmentControls = new QGroupBox(tr("Environment"), rightPanel);
    auto* environmentLayout = new QFormLayout(environmentControls);
    environmentLayout->addRow(tr("HDR"), hdrComboBox);
    environmentLayout->addRow(tr("Tilt X"), hdrTiltSpin);
    environmentLayout->addRow(tr("Tilt Y"), hdrTiltYSpin);
    environmentLayout->addRow(tr("Heading"), hdrHeadingSpin);
    environmentLayout->addRow(importHdrButton);
    rightLayout->addWidget(environmentControls);
    auto* vrControls = new QGroupBox(tr("VR View"), rightPanel);
    auto* vrLayout = new QFormLayout(vrControls);
    vrLayout->addRow(tr("Mode"), vrViewModeCombo);
    vrLayout->addRow(tr("Angle"), vrViewAngleCombo);
    vrLayout->addRow(tr("Size"), vrModelSizeSpin);
    vrLayout->addRow(tr("Distance"), vrDistanceSpin);
    vrLayout->addRow(tr("Height"), vrHeightOffsetSpin);
    vrLayout->addRow(resetVrViewButton);
    rightLayout->addWidget(vrControls);
    rightLayout->addWidget(ui->groupTreeWidget, 1);

    mainSplitter->addWidget(leftPanel);
    mainSplitter->addWidget(centerPanel);
    mainSplitter->addWidget(rightPanel);
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setStretchFactor(2, 0);
    mainSplitter->setCollapsible(0, true);
    mainSplitter->setCollapsible(2, true);
    mainSplitter->setSizes({ 210, 1120, 190 });
    setCentralWidget(central);

    QFont appFont(QStringLiteral("Segoe UI Variable"));
    appFont.setPointSize(10);
    setFont(appFont);

    setStyleSheet(QStringLiteral(
        "QMainWindow, QWidget { background: #0f172a; color: #e5e7eb; font-family: 'Segoe UI Variable', 'Segoe UI'; }"
        "QTreeView, QTreeWidget, QLineEdit, QComboBox, QDoubleSpinBox { background: #111827; color: #f8fafc; border: 1px solid #334155; border-radius: 7px; padding: 5px; }"
        "QTreeView::item, QTreeWidget::item { min-height: 25px; }"
        "QTreeView::item:selected, QTreeWidget::item:selected { background: #2563eb; color: white; }"
        "QHeaderView::section { background: #1f2937; color: #cbd5e1; border: 0; padding: 6px; }"
        "QGroupBox { font-weight: 650; border: 1px solid #334155; border-radius: 8px; margin-top: 12px; padding: 10px; background: #111827; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; color: #93c5fd; }"
        "QPushButton { background: #1f2937; color: #f8fafc; border: 1px solid #475569; border-radius: 7px; padding: 7px 10px; }"
        "QPushButton:hover { background: #26364d; border-color: #60a5fa; }"
        "QToolBar { background: #111827; border-bottom: 1px solid #334155; spacing: 6px; padding: 5px; }"
        "QLabel#panelTitle { font-size: 15px; font-weight: 750; color: #f8fafc; padding: 3px 0; }"
        "QStatusBar { background: #111827; border-top: 1px solid #334155; color: #cbd5e1; }"
        "QProgressBar { background: #020617; border: 1px solid #334155; border-radius: 6px; text-align: center; }"
        "QProgressBar::chunk { background: #38bdf8; border-radius: 6px; }"
    ));

    connect(applyTransformButton, &QPushButton::clicked, this, &MainWindow::applyTransformFromControls);
    connect(resetTransformButton, &QPushButton::clicked, this, &MainWindow::resetSelectedTransform);
    connect(treeScaleSlider, &QSlider::valueChanged, this, &MainWindow::applyTreeScale);
    connect(resetVrViewButton, &QPushButton::clicked, this, &MainWindow::resetVrView);
    connect(vrViewModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (vrThread) {
            vrThread->setViewMode(static_cast<VRRenderThread::ViewMode>(vrViewModeCombo->itemData(index).toInt()));
        }
    });
    connect(vrViewAngleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (vrThread) {
            vrThread->setViewYawDegrees(vrViewAngleCombo->itemData(index).toDouble());
        }
    });
    connect(vrModelSizeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::applyVrTuningToThread);
    connect(vrDistanceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::applyVrTuningToThread);
    connect(vrHeightOffsetSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::applyVrTuningToThread);
    connect(hdrTiltSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::applyEnvironmentOrientation);
    connect(hdrTiltYSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::applyEnvironmentOrientation);
    connect(hdrHeadingSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::applyEnvironmentOrientation);
    connect(importHdrButton, &QPushButton::clicked,
            this, &MainWindow::importHdrFile);
    connect(vrTransformCommitTimer, &QTimer::timeout,
            this, &MainWindow::commitPendingVrTransforms);
    applyTreeScale(treeScaleSlider->value());
}

void MainWindow::configureActions()
{
    ui->actionOpen_File->setText(tr("Open STL"));
    ui->actionOpen_File->setToolTip(tr("Load a single STL file"));
    ui->actionOpen_File->setIcon(makeToolbarIcon(ToolbarIcon::OpenFile));

    ui->actionOpen_Folder->setText(tr("Open STL Folder"));
    ui->actionOpen_Folder->setToolTip(tr("Load all STL files in a folder and subfolders"));
    ui->actionOpen_Folder->setIcon(makeToolbarIcon(ToolbarIcon::OpenFolder));

    ui->actionAdd_Level->setText(tr("Add Level"));
    ui->actionAdd_Item->setText(tr("Add Child"));
    ui->actionDelete_Item->setText(tr("Delete"));
    ui->actionItemOptions->setText(tr("Part Options"));
    ui->actionLock_to_New_Group->setText(tr("Lock to New Group"));
    ui->actionAdd_Level->setIcon(makeToolbarIcon(ToolbarIcon::Add, QColor(96, 165, 250)));
    ui->actionAdd_Item->setIcon(makeToolbarIcon(ToolbarIcon::Add, QColor(96, 165, 250)));
    ui->actionDelete_Item->setIcon(makeToolbarIcon(ToolbarIcon::Delete, QColor(248, 113, 113)));
    ui->actionItemOptions->setIcon(makeToolbarIcon(ToolbarIcon::Options, QColor(167, 139, 250)));
    ui->actionLock_to_New_Group->setIcon(makeToolbarIcon(ToolbarIcon::Group, QColor(45, 212, 191)));

    ui->actionStart_VR->setIcon(makeToolbarIcon(ToolbarIcon::Play, QColor(74, 222, 128)));
    ui->actionStop_VR->setIcon(makeToolbarIcon(ToolbarIcon::Stop, QColor(248, 113, 113)));
    ui->actionPause_VR->setIcon(makeToolbarIcon(ToolbarIcon::Pause, QColor(251, 191, 36)));

    ui->actionToggle_Filter->setText(tr("Visible Only"));
    ui->actionToggle_Filter->setIcon(makeToolbarIcon(ToolbarIcon::Filter, QColor(125, 211, 252)));
    ui->actionToggle_Filter->setCheckable(true);
    ui->actionToggle_Filter->setChecked(true);
    ui->actionToggle_Filter->setToolTip(tr("Hide unchecked parts in the renderer"));
    visibilityFilterOn = true;

    ui->toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    ui->toolBar->setIconSize(QSize(26, 26));
    ui->toolBar->addSeparator();

    QAction* fitViewAction = ui->toolBar->addAction(
        makeToolbarIcon(ToolbarIcon::Fit, QColor(125, 211, 252)),
        tr("Fit View"),
        this,
        &MainWindow::fitCameraToScene);
    fitViewAction->setToolTip(tr("Center the camera on the loaded model"));

    QAction* undoAction = undoStack->createUndoAction(this, tr("Undo"));
    undoAction->setIcon(makeToolbarIcon(ToolbarIcon::Undo, QColor(148, 163, 184)));
    QAction* redoAction = undoStack->createRedoAction(this, tr("Redo"));
    redoAction->setIcon(makeToolbarIcon(ToolbarIcon::Redo, QColor(148, 163, 184)));
    ui->toolBar->addAction(undoAction);
    ui->toolBar->addAction(redoAction);

    QAction* renameAction = ui->toolBar->addAction(
        makeToolbarIcon(ToolbarIcon::Options, QColor(167, 139, 250)),
        tr("Rename"),
        this,
        [this]() {
            QModelIndex index = normalizedTreeIndex(ui->treeView->currentIndex());
            if (index.isValid()) {
                ui->treeView->edit(index);
            }
        });
    renameAction->setShortcut(QKeySequence(Qt::Key_F2));
    renameAction->setToolTip(tr("Rename selected tree item"));
    addAction(renameAction);

    gridAction = ui->toolBar->addAction(makeToolbarIcon(ToolbarIcon::Grid, QColor(45, 212, 191)), tr("Grid"));
    gridAction->setCheckable(true);
    gridAction->setChecked(true);
    gridAction->setToolTip(tr("Show or hide the floor placement grid"));
    connect(gridAction, &QAction::toggled, this, [this](bool checked) {
        if (gridActor) {
            gridActor->SetVisibility(checked);
            renderWindow->Render();
        }
    });

    performanceAction = ui->toolBar->addAction(makeToolbarIcon(ToolbarIcon::Performance, QColor(250, 204, 21)), tr("Fast Mode"));
    performanceAction->setCheckable(true);
    performanceAction->setChecked(true);
    performanceAction->setToolTip(tr("Prioritize smooth interaction for large STL assemblies"));
    connect(performanceAction, &QAction::toggled, this, &MainWindow::applyPerformanceMode);

    showcaseSpinAction = ui->toolBar->addAction(
        makeToolbarIcon(ToolbarIcon::Showcase, QColor(45, 212, 191)),
        tr("Showcase Spin"));
    showcaseSpinAction->setCheckable(true);
    showcaseSpinAction->setToolTip(tr("Slowly rotate the car in VR for a presentation-style view"));
    connect(showcaseSpinAction, &QAction::toggled, this, [this](bool enabled) {
        if (vrThread) {
            vrThread->issueCommand(VRRenderThread::SET_SHOWCASE_SPIN, enabled ? 1.0 : 0.0);
        }
    });

    QAction* focusViewAction = ui->toolBar->addAction(
        makeToolbarIcon(ToolbarIcon::Fit, QColor(34, 197, 94)),
        tr("Focus View"),
        this,
        [this]() {
            if (mainSplitter) {
                mainSplitter->setSizes({ 72, std::max(420, width() - 144), 72 });
            }
        });
    focusViewAction->setToolTip(tr("Shrink side panels and give most space to the STL view"));

    QAction* clearAction = ui->toolBar->addAction(
        makeToolbarIcon(ToolbarIcon::Delete, QColor(248, 113, 113)),
        tr("Clear"),
        this,
        [this]() {
            if (partList->rowCount(QModelIndex()) == 0) return;
            const auto reply = QMessageBox::question(
                this,
                tr("Clear Model"),
                tr("Remove all loaded parts and groups from the scene?"),
                QMessageBox::Yes | QMessageBox::No);

            if (reply != QMessageBox::Yes) return;

            undoStack->clear();
            selectedViewPart = nullptr;
            hoveredPart = nullptr;
            syncTransformControls(nullptr);
            selectionDetailsLabel->setText(tr("No part selected"));

            while (partList->rowCount(QModelIndex()) > 0) {
                partList->removePart(partList->index(0, 0, QModelIndex()));
            }
            ui->groupTreeWidget->clear();
            rebuildScene(true);
            updateSceneSummary();
            emit statusUpdateMessage(tr("Scene cleared."), 3000);
        });
    clearAction->setToolTip(tr("Remove all loaded STL parts"));

    ui->treeView->addAction(ui->actionOpen_File);
    ui->treeView->addAction(ui->actionOpen_Folder);
    ui->treeView->addAction(ui->actionItemOptions);
    ui->treeView->addAction(ui->actionAdd_Level);
    ui->treeView->addAction(ui->actionAdd_Item);
    ui->treeView->addAction(ui->actionDelete_Item);
    ui->treeView->addAction(ui->actionLock_to_New_Group);
    ui->treeView->addAction(renameAction);

    QAction* actionUnlock = new QAction(tr("Unlock / Remove from Group"), this);
    ui->groupTreeWidget->addAction(actionUnlock);
    connect(actionUnlock, &QAction::triggered, this, &MainWindow::on_actionUnlock_Item_triggered);
}

void MainWindow::setupRenderingPipeline()
{
    renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    renderWindow->SetMultiSamples(0);
    ui->vtkWidget->setRenderWindow(renderWindow);

    renderer = vtkSmartPointer<vtkRenderer>::New();
    renderWindow->AddRenderer(renderer);
    renderer->GradientBackgroundOn();
    renderer->SetBackground(0.04, 0.07, 0.13);
    renderer->SetBackground2(0.12, 0.18, 0.28);

    vtkSmartPointer<vtkLight> mainLight = vtkSmartPointer<vtkLight>::New();
    mainLight->SetLightTypeToSceneLight();
    mainLight->SetPosition(120, 140, 160);
    mainLight->SetFocalPoint(0, 0, 0);
    mainLight->SetIntensity(1.1);
    renderer->AddLight(mainLight);

    vtkSmartPointer<vtkLight> fillLight = vtkSmartPointer<vtkLight>::New();
    fillLight->SetLightTypeToSceneLight();
    fillLight->SetPosition(-140, -80, 120);
    fillLight->SetFocalPoint(0, 0, 0);
    fillLight->SetIntensity(0.45);
    renderer->AddLight(fillLight);

    vtkNew<vtkInteractorStyleTrackballCamera> cameraStyle;
    ui->vtkWidget->interactor()->SetInteractorStyle(cameraStyle);
    ui->vtkWidget->interactor()->SetDesiredUpdateRate(45.0);
    ui->vtkWidget->interactor()->SetStillUpdateRate(8.0);

    vtkNew<vtkAxesActor> axes;
    orientationMarker = vtkSmartPointer<vtkOrientationMarkerWidget>::New();
    orientationMarker->SetOrientationMarker(axes);
    orientationMarker->SetInteractor(ui->vtkWidget->interactor());
    orientationMarker->SetViewport(0.0, 0.0, 0.16, 0.16);
    orientationMarker->SetEnabled(1);
    orientationMarker->InteractiveOff();

    createGridActor();
    addPersistentSceneProps();

    propPicker = vtkSmartPointer<vtkPropPicker>::New();
    propPicker->PickFromListOn();
    hoverPickTimer.start();
    mouseMoveCallback = vtkSmartPointer<vtkCallbackCommand>::New();
    mouseMoveCallback->SetClientData(this);
    mouseMoveCallback->SetCallback([](vtkObject*, unsigned long, void* clientData, void*) {
        static_cast<MainWindow*>(clientData)->handleVtkHoverEvent();
    });
    leftClickCallback = vtkSmartPointer<vtkCallbackCommand>::New();
    leftClickCallback->SetClientData(this);
    leftClickCallback->SetCallback([](vtkObject*, unsigned long, void* clientData, void*) {
        static_cast<MainWindow*>(clientData)->handleVtkClickEvent();
    });
    ui->vtkWidget->interactor()->AddObserver(vtkCommand::MouseMoveEvent, mouseMoveCallback);
    ui->vtkWidget->interactor()->AddObserver(vtkCommand::LeftButtonPressEvent, leftClickCallback);

    populateHdrOptions();
    connect(hdrComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::applyEnvironmentSelection);
    applyEnvironmentOrientation();

    applyPerformanceMode(!performanceAction || performanceAction->isChecked());
    renderWindow->Render();
}

void MainWindow::createGridActor()
{
    vtkNew<vtkPoints> points;
    vtkNew<vtkCellArray> lines;

    const int halfLines = 20;
    const double spacing = 25.0;
    vtkIdType pointId = 0;

    for (int i = -halfLines; i <= halfLines; ++i) {
        const double coord = i * spacing;

        points->InsertNextPoint(-halfLines * spacing, coord, 0.0);
        points->InsertNextPoint(halfLines * spacing, coord, 0.0);
        vtkIdType xLine[2] = { pointId, pointId + 1 };
        lines->InsertNextCell(2, xLine);
        pointId += 2;

        points->InsertNextPoint(coord, -halfLines * spacing, 0.0);
        points->InsertNextPoint(coord, halfLines * spacing, 0.0);
        vtkIdType yLine[2] = { pointId, pointId + 1 };
        lines->InsertNextCell(2, yLine);
        pointId += 2;
    }

    vtkNew<vtkPolyData> gridData;
    gridData->SetPoints(points);
    gridData->SetLines(lines);

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(gridData);

    gridActor = vtkSmartPointer<vtkActor>::New();
    gridActor->SetMapper(mapper);
    gridActor->GetProperty()->SetColor(0.22, 0.33, 0.45);
    gridActor->GetProperty()->SetOpacity(0.55);
    gridActor->GetProperty()->SetLineWidth(1.0);
    gridActor->PickableOff();
}

void MainWindow::addPersistentSceneProps()
{
    if (mySkybox) {
        renderer->AddActor(mySkybox);
    }

    if (environmentActor) {
        renderer->AddActor(environmentActor);
    }

    if (gridActor && (!gridAction || gridAction->isChecked())) {
        renderer->AddActor(gridActor);
    }
}

void MainWindow::populateHdrOptions()
{
    hdrComboBox->blockSignals(true);
    hdrComboBox->clear();
    hdrComboBox->addItem(tr("Default Studio Gradient"), QString());

    QDir dir(QCoreApplication::applicationDirPath());
    QStringList roots;
    roots << QDir::currentPath()
          << QCoreApplication::applicationDirPath()
          << QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("..")
          << QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../..")
          << QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../../..");

    QSet<QString> added;
    for (const QString& root : roots) {
        QDirIterator iterator(root, QStringList() << "*.hdr" << "*.HDR", QDir::Files, QDirIterator::NoIteratorFlags);
        while (iterator.hasNext()) {
            const QFileInfo info(iterator.next());
            const QString path = info.absoluteFilePath();
            if (added.contains(path)) continue;
            added.insert(path);
            hdrComboBox->addItem(info.completeBaseName().replace('_', ' '), path);
        }
    }

    QSettings settings("EngineeringGroup4", "VRModelViewer");
    const QStringList customHdrFiles = settings.value("customHdrFiles").toStringList();
    for (const QString& customPath : customHdrFiles) {
        const QFileInfo info(customPath);
        const QString path = info.absoluteFilePath();
        if (!info.exists() || added.contains(path)) continue;
        added.insert(path);
        hdrComboBox->addItem(info.completeBaseName().replace('_', ' '), path);
    }

    hdrComboBox->blockSignals(false);
}

void MainWindow::importHdrFile()
{
    QSettings settings("EngineeringGroup4", "VRModelViewer");
    const QString startDir = settings.value("lastHdrDir", QDir::homePath()).toString();
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Add HDR Environment"),
        startDir,
        tr("HDR Environment Files (*.hdr *.HDR)")
    );

    if (fileName.isEmpty()) {
        return;
    }

    const QFileInfo info(fileName);
    const QString path = info.absoluteFilePath();
    settings.setValue("lastHdrDir", info.absolutePath());

    QStringList customHdrFiles = settings.value("customHdrFiles").toStringList();
    customHdrFiles.removeAll(path);
    customHdrFiles.prepend(path);
    settings.setValue("customHdrFiles", customHdrFiles);

    int existingIndex = hdrComboBox->findData(path);
    if (existingIndex < 0) {
        hdrComboBox->addItem(info.completeBaseName().replace('_', ' '), path);
        existingIndex = hdrComboBox->count() - 1;
    }

    hdrComboBox->setCurrentIndex(existingIndex);
    emit statusUpdateMessage(tr("Added HDR environment %1").arg(info.fileName()), 2500);
}

void MainWindow::applyEnvironmentSelection(int index)
{
    const QString path = hdrComboBox->itemData(index).toString();

    if (path.isEmpty()) {
        mySkybox = nullptr;
        environmentActor = nullptr;
        environmentTexture = nullptr;
        environmentBaseImage = nullptr;
        environmentImage = nullptr;
        renderer->UseImageBasedLightingOff();
        renderer->SetEnvironmentTexture(nullptr);
        renderer->SetBackgroundTexture(nullptr);
        renderer->TexturedBackgroundOff();
        renderer->GradientBackgroundOn();
        renderer->SetBackground(0.04, 0.07, 0.13);
        renderer->SetBackground2(0.12, 0.18, 0.28);
        applyEnvironmentOrientation();
        if (vrThread) {
            vrThread->setEnvironmentPath(QString());
        }
        rebuildScene(false);
        emit statusUpdateMessage(tr("Using default studio background."), 2500);
        return;
    }

    vtkSmartPointer<vtkHDRReader> reader = vtkSmartPointer<vtkHDRReader>::New();
    reader->SetFileName(path.toStdString().c_str());
    reader->Update();

    vtkImageData* image = reader->GetOutput();
    if (!image || image->GetNumberOfPoints() == 0) {
        QMessageBox::warning(this, tr("HDR Load Failed"),
                             tr("The selected HDR file could not be loaded:\n%1").arg(path));
        hdrComboBox->setCurrentIndex(0);
        return;
    }

    environmentTexture = vtkSmartPointer<vtkTexture>::New();
    environmentBaseImage = toneMapHdrForTexture(image);
    environmentImage = environmentBaseImage;
    environmentTexture->SetInputData(environmentImage);
    environmentTexture->SetColorModeToDirectScalars();
    environmentTexture->MipmapOn();
    environmentTexture->InterpolateOn();

    environmentActor = nullptr;
    mySkybox = vtkSmartPointer<vtkSkybox>::New();
    mySkybox->SetProjectionToSphere();
    mySkybox->SetTexture(environmentTexture);
    mySkybox->PickableOff();
    mySkybox->GammaCorrectOff();
    applyEnvironmentOrientation();

    renderer->UseImageBasedLightingOn();
    renderer->SetEnvironmentTexture(environmentTexture, true);
    renderer->SetBackgroundTexture(nullptr);
    renderer->TexturedBackgroundOff();
    renderer->UseImageBasedLightingOn();
    renderer->SetEnvironmentTexture(environmentTexture);
    renderer->GradientBackgroundOff();
    if (vrThread) {
        vrThread->setEnvironmentPath(path);
    }
    rebuildScene(false);
    emit statusUpdateMessage(tr("Environment set to %1").arg(QFileInfo(path).completeBaseName()), 2500);
}

void MainWindow::applyEnvironmentOrientation()
{
    if (!renderer) {
        return;
    }

    const double tiltDegrees = hdrTiltSpin ? hdrTiltSpin->value() : 0.0;
    const double tiltYDegrees = hdrTiltYSpin ? hdrTiltYSpin->value() : 0.0;
    const double correctedRollDegrees = kHdrUprightRollDegrees + tiltDegrees;
    const double headingDegrees = hdrHeadingSpin ? hdrHeadingSpin->value() : 0.0;

    renderer->SetEnvironmentUp(0.0, 0.0, 1.0);
    renderer->SetEnvironmentRight(1.0, 0.0, 0.0);

    if (environmentBaseImage && environmentTexture) {
        environmentImage = orientedEnvironmentImage(environmentBaseImage, correctedRollDegrees, tiltYDegrees, headingDegrees);
        environmentTexture->SetInputData(environmentImage);
        environmentTexture->Modified();
        renderer->SetEnvironmentTexture(environmentTexture, true);
    }

    if (mySkybox) {
        mySkybox->SetOrientation(0.0, 0.0, 0.0);
        mySkybox->Modified();
    }

    if (vrThread) {
        vrThread->setEnvironmentOrientation(tiltDegrees, tiltYDegrees, headingDegrees);
    }

    if (renderWindow) {
        renderWindow->Render();
    }
}

QModelIndex MainWindow::normalizedTreeIndex(const QModelIndex& index) const
{
    if (!index.isValid()) return QModelIndex();
    return index.column() > 0 ? index.siblingAtColumn(0) : index;
}

void MainWindow::rebuildScene(bool resetCamera)
{
    renderer->RemoveAllViewProps();
    actorPartMap.clear();
    if (propPicker) {
        propPicker->InitializePickList();
        propPicker->PickFromListOn();
    }
    addPersistentSceneProps();

    QMap<QString, vtkSmartPointer<vtkAssembly>> assemblies;
    updateRenderFromTree(QModelIndex(), assemblies);

    for (auto assembly : assemblies.values()) {
        renderer->AddActor(assembly);
    }

    if (resetCamera) {
        fitCameraToScene();
    }
    else {
        renderer->ResetCameraClippingRange();
        renderWindow->Render();
    }
}

void MainWindow::fitCameraToScene()
{
    if (!renderer || !renderWindow) return;

    renderer->ResetCamera();
    vtkCamera* camera = renderer->GetActiveCamera();
    if (camera) {
        camera->Azimuth(28.0);
        camera->Elevation(18.0);
        camera->Dolly(1.12);
    }
    renderer->ResetCameraClippingRange();

    const QSignalBlocker blocker(ui->zoomSlider);
    ui->zoomSlider->setValue(100);
    renderWindow->Render();
}

void MainWindow::applyTreeScale(int value)
{
    const double scale = value / 100.0;
    QFont treeFont = font();
    treeFont.setPointSizeF(10.0 * scale);

    ui->treeView->setFont(treeFont);
    ui->treeView->header()->setFont(treeFont);
    ui->treeView->setIndentation(qRound(20.0 * scale));
    ui->treeView->setIconSize(QSize(qRound(18.0 * scale), qRound(18.0 * scale)));
    ui->treeView->setStyleSheet(QStringLiteral("QTreeView::item { min-height: %1px; }")
        .arg(qRound(25.0 * scale)));
}

void MainWindow::applyPerformanceMode(bool enabled)
{
    hoverPickIntervalMs = enabled ? 65 : 30;

    if (renderWindow) {
        renderWindow->SetMultiSamples(enabled ? 0 : 4);
    }

    if (ui && ui->vtkWidget && ui->vtkWidget->interactor()) {
        ui->vtkWidget->interactor()->SetDesiredUpdateRate(enabled ? 60.0 : 30.0);
        ui->vtkWidget->interactor()->SetStillUpdateRate(enabled ? 10.0 : 5.0);
    }

    if (renderWindow) {
    }
}
    void MainWindow::applyVrTuningToThread()
    {
        if (!vrThread) {
            return;
        }

        vrThread->setViewTuning(
            vrModelSizeSpin->value(),
            vrDistanceSpin->value(),
            vrHeightOffsetSpin->value()
        );
    }

bool MainWindow::applyTreeFilter(const QModelIndex& parentIndex, const QString& filterText)
{
    bool anyVisible = false;
    const int rows = partList->rowCount(parentIndex);

    for (int row = 0; row < rows; ++row) {
        QModelIndex childIndex = partList->index(row, 0, parentIndex);
        ModelPart* part = static_cast<ModelPart*>(childIndex.internalPointer());

        const bool childVisible = applyTreeFilter(childIndex, filterText);
        const bool selfVisible = filterText.isEmpty()
            || part->data(0).toString().contains(filterText, Qt::CaseInsensitive);
        const bool visible = selfVisible || childVisible;

        ui->treeView->setRowHidden(row, parentIndex, !visible);
        if (visible && !filterText.isEmpty()) {
            ui->treeView->expand(childIndex);
        }

        anyVisible = anyVisible || visible;
    }

    return anyVisible;
}

void MainWindow::handleSearchTextChanged(const QString& text)
{
    applyTreeFilter(QModelIndex(), text.trimmed());
}

void MainWindow::collectSceneStats(const QModelIndex& parentIndex, int& loadedParts, int& visibleParts, qint64& triangles) const
{
    const int rows = partList->rowCount(parentIndex);
    for (int row = 0; row < rows; ++row) {
        QModelIndex childIndex = partList->index(row, 0, parentIndex);
        ModelPart* part = static_cast<ModelPart*>(childIndex.internalPointer());

        if (part->hasGeometry()) {
            ++loadedParts;
            if (part->visible()) {
                ++visibleParts;
            }
            triangles += part->triangleCount();
        }

        collectSceneStats(childIndex, loadedParts, visibleParts, triangles);
    }
}

bool MainWindow::collectModelBounds(const QModelIndex& parentIndex, double bounds[6]) const
{
    bool found = false;
    const int rows = partList->rowCount(parentIndex);

    for (int row = 0; row < rows; ++row) {
        QModelIndex childIndex = partList->index(row, 0, parentIndex);
        ModelPart* part = static_cast<ModelPart*>(childIndex.internalPointer());

        if (part && part->hasGeometry() && part->getActor()) {
            double actorBounds[6];
            part->getActor()->GetBounds(actorBounds);

            if (!found) {
                std::copy(actorBounds, actorBounds + 6, bounds);
                found = true;
            }
            else {
                bounds[0] = std::min(bounds[0], actorBounds[0]);
                bounds[1] = std::max(bounds[1], actorBounds[1]);
                bounds[2] = std::min(bounds[2], actorBounds[2]);
                bounds[3] = std::max(bounds[3], actorBounds[3]);
                bounds[4] = std::min(bounds[4], actorBounds[4]);
                bounds[5] = std::max(bounds[5], actorBounds[5]);
            }
        }

        double childBounds[6];
        if (collectModelBounds(childIndex, childBounds)) {
            if (!found) {
                std::copy(childBounds, childBounds + 6, bounds);
                found = true;
            }
            else {
                bounds[0] = std::min(bounds[0], childBounds[0]);
                bounds[1] = std::max(bounds[1], childBounds[1]);
                bounds[2] = std::min(bounds[2], childBounds[2]);
                bounds[3] = std::max(bounds[3], childBounds[3]);
                bounds[4] = std::min(bounds[4], childBounds[4]);
                bounds[5] = std::max(bounds[5], childBounds[5]);
            }
        }
    }

    return found;
}

void MainWindow::translateModelParts(const QModelIndex& parentIndex, double dx, double dy, double dz)
{
    const int rows = partList->rowCount(parentIndex);
    for (int row = 0; row < rows; ++row) {
        QModelIndex childIndex = partList->index(row, 0, parentIndex);
        ModelPart* part = static_cast<ModelPart*>(childIndex.internalPointer());

        if (part && part->hasGeometry()) {
            double x, y, z;
            part->getPosition(x, y, z);
            part->setPosition(x + dx, y + dy, z + dz);
        }

        translateModelParts(childIndex, dx, dy, dz);
    }
}

void MainWindow::centerModelAtOrigin(const QModelIndex& parentIndex)
{
    double bounds[6];
    if (!collectModelBounds(parentIndex, bounds)) {
        return;
    }

    const double centerX = (bounds[0] + bounds[1]) * 0.5;
    const double centerY = (bounds[2] + bounds[3]) * 0.5;
    const double floorZ = bounds[4];

    translateModelParts(parentIndex, -centerX, -centerY, -floorZ);
}

void MainWindow::updateSceneSummary()
{
    int loadedParts = 0;
    int visibleParts = 0;
    qint64 triangles = 0;
    collectSceneStats(QModelIndex(), loadedParts, visibleParts, triangles);

    const QLocale locale;
    sceneStatsLabel->setText(tr("%1 parts | %2 visible | %3 triangles")
        .arg(locale.toString(loadedParts))
        .arg(locale.toString(visibleParts))
        .arg(locale.toString(triangles)));
}

QList<ModelPart*> MainWindow::selectedTransformTargets() const
{
    QList<ModelPart*> targets;
    if (!selectedViewPart) return targets;

    const QString groupName = selectedViewPart->getGroupName();
    if (!groupName.isEmpty()) {
        targets = const_cast<MainWindow*>(this)->getGroupMembers(QModelIndex(), groupName);
    }

    if (targets.isEmpty()) {
        targets.append(selectedViewPart);
    }
    return targets;
}

void MainWindow::syncTransformControls(ModelPart* part)
{
    const bool enabled = part != nullptr;
    for (int i = 0; i < 3; ++i) {
        positionSpin[i]->setEnabled(enabled);
        rotationSpin[i]->setEnabled(enabled);
    }
    transformScaleSpin->setEnabled(enabled);
    applyTransformButton->setEnabled(enabled);
    resetTransformButton->setEnabled(enabled);

    if (!part) return;

    double px, py, pz, rx, ry, rz;
    part->getPosition(px, py, pz);
    part->getRotation(rx, ry, rz);

    const QSignalBlocker b0(positionSpin[0]);
    const QSignalBlocker b1(positionSpin[1]);
    const QSignalBlocker b2(positionSpin[2]);
    const QSignalBlocker b3(rotationSpin[0]);
    const QSignalBlocker b4(rotationSpin[1]);
    const QSignalBlocker b5(rotationSpin[2]);
    const QSignalBlocker b6(transformScaleSpin);

    positionSpin[0]->setValue(px);
    positionSpin[1]->setValue(py);
    positionSpin[2]->setValue(pz);
    rotationSpin[0]->setValue(rx);
    rotationSpin[1]->setValue(ry);
    rotationSpin[2]->setValue(rz);
    transformScaleSpin->setValue(part->getScale());
}

void MainWindow::applyTransformStates(const QList<ModelPart*>& parts, const QVector<ModelPart::TransformState>& states)
{
    for (int i = 0; i < parts.size() && i < states.size(); ++i) {
        if (parts[i]) {
            parts[i]->setTransformState(states[i]);
        }
    }

    syncTransformControls(selectedViewPart);
    renderer->ResetCameraClippingRange();
    renderWindow->Render();
}

void MainWindow::applyTransformFromControls()
{
    if (!selectedViewPart) return;

    QList<ModelPart*> targets = selectedTransformTargets();
    if (targets.isEmpty()) return;

    QVector<ModelPart::TransformState> before;
    QVector<ModelPart::TransformState> after;
    before.reserve(targets.size());
    after.reserve(targets.size());

    const ModelPart::TransformState selectedBefore = selectedViewPart->transformState();
    ModelPart::TransformState selectedAfter = selectedBefore;
    selectedAfter.position = { positionSpin[0]->value(), positionSpin[1]->value(), positionSpin[2]->value() };
    selectedAfter.rotation = { rotationSpin[0]->value(), rotationSpin[1]->value(), rotationSpin[2]->value() };
    selectedAfter.scale = transformScaleSpin->value();

    const double dx = selectedAfter.position[0] - selectedBefore.position[0];
    const double dy = selectedAfter.position[1] - selectedBefore.position[1];
    const double dz = selectedAfter.position[2] - selectedBefore.position[2];
    const double drx = selectedAfter.rotation[0] - selectedBefore.rotation[0];
    const double dry = selectedAfter.rotation[1] - selectedBefore.rotation[1];
    const double drz = selectedAfter.rotation[2] - selectedBefore.rotation[2];
    const double scaleRatio = selectedBefore.scale == 0.0 ? 1.0 : selectedAfter.scale / selectedBefore.scale;

    for (ModelPart* part : targets) {
        ModelPart::TransformState state = part->transformState();
        before.append(state);

        if (part == selectedViewPart) {
            after.append(selectedAfter);
        }
        else {
            state.position[0] += dx;
            state.position[1] += dy;
            state.position[2] += dz;
            state.rotation[0] += drx;
            state.rotation[1] += dry;
            state.rotation[2] += drz;
            state.scale *= scaleRatio;
            after.append(state);
        }
    }

    undoStack->push(new LambdaUndoCommand(
        tr("Transform %1").arg(selectedViewPart->data(0).toString()),
        [this, targets, before]() { applyTransformStates(targets, before); },
        [this, targets, after]() { applyTransformStates(targets, after); }));
}

void MainWindow::resetSelectedTransform()
{
    if (!selectedViewPart) return;

    for (int i = 0; i < 3; ++i) {
        positionSpin[i]->setValue(0.0);
        rotationSpin[i]->setValue(0.0);
    }
    transformScaleSpin->setValue(1.0);
    applyTransformFromControls();
}

void MainWindow::setHoveredPart(ModelPart* part)
{
    if (hoveredPart == part) return;

    if (hoveredPart) {
        hoveredPart->setHighlighted(false);
        QModelIndex oldIndex = findIndexForPart(hoveredPart);
        if (oldIndex.isValid()) {
            emit partList->dataChanged(oldIndex, oldIndex.siblingAtColumn(1), { Qt::BackgroundRole });
        }
    }

    hoveredPart = part;
    if (hoveredPart) {
        hoveredPart->setHighlighted(true);
        QModelIndex newIndex = findIndexForPart(hoveredPart);
        if (newIndex.isValid()) {
            emit partList->dataChanged(newIndex, newIndex.siblingAtColumn(1), { Qt::BackgroundRole });
        }
        selectionDetailsLabel->setText(hoveredPart->summary());
    }

    renderWindow->Render();
}

void MainWindow::selectViewPart(ModelPart* part)
{
    if (selectedViewPart == part && part) {
        syncTransformControls(part);
        return;
    }

    if (selectedViewPart) {
        selectedViewPart->setSelectedInView(false);
        QModelIndex oldIndex = findIndexForPart(selectedViewPart);
        if (oldIndex.isValid()) {
            emit partList->dataChanged(oldIndex, oldIndex.siblingAtColumn(1), { Qt::BackgroundRole });
        }
    }

    selectedViewPart = part;
    if (selectedViewPart) {
        selectedViewPart->setSelectedInView(true);
        QModelIndex index = findIndexForPart(selectedViewPart);
        if (index.isValid()) {
            ui->treeView->setCurrentIndex(index);
            ui->treeView->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            emit partList->dataChanged(index, index.siblingAtColumn(1), { Qt::BackgroundRole });
            ui->treeView->scrollTo(index, QAbstractItemView::PositionAtCenter);
        }
        selectionDetailsLabel->setText(selectedViewPart->summary());
    }

    syncTransformControls(selectedViewPart);
    renderWindow->Render();
}

QModelIndex MainWindow::findIndexForPart(ModelPart* part) const
{
    return findIndexForPartRecursive(QModelIndex(), part);
}

QModelIndex MainWindow::findIndexForPartRecursive(const QModelIndex& parentIndex, ModelPart* part) const
{
    const int rows = partList->rowCount(parentIndex);
    for (int row = 0; row < rows; ++row) {
        QModelIndex childIndex = partList->index(row, 0, parentIndex);
        if (childIndex.internalPointer() == part) {
            return childIndex;
        }
        QModelIndex nested = findIndexForPartRecursive(childIndex, part);
        if (nested.isValid()) {
            return nested;
        }
    }
    return QModelIndex();
}

ModelPart* MainWindow::findPartById(const QString& id) const
{
    return findPartByIdRecursive(QModelIndex(), id);
}

ModelPart* MainWindow::findPartByIdRecursive(const QModelIndex& parentIndex, const QString& id) const
{
    const int rows = partList->rowCount(parentIndex);
    for (int row = 0; row < rows; ++row) {
        QModelIndex childIndex = partList->index(row, 0, parentIndex);
        ModelPart* part = static_cast<ModelPart*>(childIndex.internalPointer());
        if (part && part->id() == id) {
            return part;
        }
        if (ModelPart* nested = findPartByIdRecursive(childIndex, id)) {
            return nested;
        }
    }
    return nullptr;
}

void MainWindow::handleVtkHoverEvent()
{
    if (!propPicker || !ui->vtkWidget->interactor()) return;
    if (hoverPickTimer.isValid() && hoverPickTimer.elapsed() < hoverPickIntervalMs) return;
    hoverPickTimer.restart();

    int* eventPosition = ui->vtkWidget->interactor()->GetEventPosition();
    propPicker->Pick(eventPosition[0], eventPosition[1], 0, renderer);
    vtkActor* actor = propPicker->GetActor();
    setHoveredPart(actorPartMap.value(actor, nullptr));
}

void MainWindow::handleVtkClickEvent()
{
    if (!propPicker || !ui->vtkWidget->interactor()) return;

    int* eventPosition = ui->vtkWidget->interactor()->GetEventPosition();
    propPicker->Pick(eventPosition[0], eventPosition[1], 0, renderer);
    vtkActor* actor = propPicker->GetActor();
    if (ModelPart* part = actorPartMap.value(actor, nullptr)) {
        selectViewPart(part);
        emit statusUpdateMessage(tr("Selected %1").arg(part->data(0).toString()), 2500);
    }
}

void MainWindow::applyVrTransformUpdate(const QString& partId, double px, double py, double pz,
                                        double rx, double ry, double rz, double scale)
{
    ModelPart* part = findPartById(partId);
    if (!part) return;

    const ModelPart::TransformState before = part->transformState();
    ModelPart::TransformState after;
    after.position = { px, py, pz };
    after.rotation = { rx, ry, rz };
    after.scale = scale;

    const bool changed =
        std::abs(before.position[0] - after.position[0]) > 0.001 ||
        std::abs(before.position[1] - after.position[1]) > 0.001 ||
        std::abs(before.position[2] - after.position[2]) > 0.001 ||
        std::abs(before.rotation[0] - after.rotation[0]) > 0.01 ||
        std::abs(before.rotation[1] - after.rotation[1]) > 0.01 ||
        std::abs(before.rotation[2] - after.rotation[2]) > 0.01 ||
        std::abs(before.scale - after.scale) > 0.001;

    if (!changed) {
        return;
    }

    if (!pendingVrTransformStartStates.contains(partId)) {
        pendingVrTransformStartStates.insert(partId, before);
    }
    pendingVrTransformLatestStates.insert(partId, after);

    part->setTransformState(after);

    if (part == selectedViewPart) {
        syncTransformControls(part);
    }

    if (renderWindow && (!vrTransformRenderTimer.isValid() || vrTransformRenderTimer.elapsed() > 33)) {
        renderer->ResetCameraClippingRange();
        renderWindow->Render();
        vrTransformRenderTimer.restart();
    }

    if (vrTransformCommitTimer) {
        vrTransformCommitTimer->start(350);
    }
}

void MainWindow::commitPendingVrTransforms()
{
    const auto pendingIds = pendingVrTransformLatestStates.keys();
    for (const QString& partId : pendingIds) {
        ModelPart* part = findPartById(partId);
        if (!part) {
            continue;
        }

        const ModelPart::TransformState before = pendingVrTransformStartStates.value(partId);
        const ModelPart::TransformState after = pendingVrTransformLatestStates.value(partId);

        const bool changed =
            std::abs(before.position[0] - after.position[0]) > 0.001 ||
            std::abs(before.position[1] - after.position[1]) > 0.001 ||
            std::abs(before.position[2] - after.position[2]) > 0.001 ||
            std::abs(before.rotation[0] - after.rotation[0]) > 0.01 ||
            std::abs(before.rotation[1] - after.rotation[1]) > 0.01 ||
            std::abs(before.rotation[2] - after.rotation[2]) > 0.01 ||
            std::abs(before.scale - after.scale) > 0.001;

        if (!changed) {
            continue;
        }

        QList<ModelPart*> targets;
        targets.append(part);
        QVector<ModelPart::TransformState> beforeStates;
        beforeStates.append(before);
        QVector<ModelPart::TransformState> afterStates;
        afterStates.append(after);

        undoStack->push(new LambdaUndoCommand(
            tr("VR transform %1").arg(part->data(0).toString()),
            [this, targets, beforeStates]() { applyTransformStates(targets, beforeStates); },
            [this, targets, afterStates]() { applyTransformStates(targets, afterStates); }));
    }

    pendingVrTransformStartStates.clear();
    pendingVrTransformLatestStates.clear();
}

void MainWindow::applyVrColorUpdate(const QString& partId, int r, int g, int b)
{
    ModelPart* part = findPartById(partId);
    if (!part) return;

    part->setColour(
        static_cast<unsigned char>(std::clamp(r, 0, 255)),
        static_cast<unsigned char>(std::clamp(g, 0, 255)),
        static_cast<unsigned char>(std::clamp(b, 0, 255)));

    QModelIndex index = findIndexForPart(part);
    if (index.isValid()) {
        emit partList->dataChanged(index, index.siblingAtColumn(1),
            { Qt::DisplayRole, Qt::BackgroundRole, Qt::ToolTipRole });
    }

    if (renderWindow) {
        renderWindow->Render();
    }
    emit statusUpdateMessage(tr("VR controller changed colour for %1").arg(part->data(0).toString()), 2500);
}

void MainWindow::resetVrView()
{
    if (!vrThread) {
        emit statusUpdateMessage(tr("Start VR first, then reset the headset view."), 3000);
        return;
    }

    double bounds[6];
    if (collectModelBounds(QModelIndex(), bounds)) {
        vrThread->setSceneBounds(bounds);
    }
    vrThread->setViewMode(static_cast<VRRenderThread::ViewMode>(vrViewModeCombo->currentData().toInt()));
    vrThread->setViewYawDegrees(vrViewAngleCombo->currentData().toDouble());
    vrThread->setViewTuning(
        vrModelSizeSpin->value(),
        vrDistanceSpin->value(),
        vrHeightOffsetSpin->value());
    vrThread->issueCommand(VRRenderThread::RESET_VIEW, 0.0);
    emit statusUpdateMessage(tr("VR view reset."), 2500);
}

void MainWindow::stopVRThread()
{
    if (!vrThread) return;

    if (vrTransformCommitTimer) {
        vrTransformCommitTimer->stop();
    }
    commitPendingVrTransforms();

    if (vrThread->isRunning()) {
        vrThread->issueCommand(VRRenderThread::END_RENDER, 0);
        if (!vrThread->wait(5000)) {
            vrThread->terminate();
            vrThread->wait(2000);
        }
    }

    delete vrThread;
    vrThread = nullptr;
}

void MainWindow::handleButton() {
    QModelIndex index = normalizedTreeIndex(ui->treeView->currentIndex());

    QList<QVariant> newData;
    newData << "New Part" << "Visible";

    QModelIndex newChildIndex = partList->appendChild(index, newData);

    if (newChildIndex.isValid()) {
        if (index.isValid()) {
            ui->treeView->expand(index);
        }
        ui->treeView->setCurrentIndex(newChildIndex);
    }

    emit statusUpdateMessage(QString("Added new child to ") +
        (index.isValid() ? static_cast<ModelPart*>(index.internalPointer())->data(0).toString() : "Root"), 0);
}

void MainWindow::handleTreeClicked() {
    QModelIndex index = normalizedTreeIndex(ui->treeView->currentIndex());
    if (!index.isValid()) return;

    ModelPart* selectedPart = static_cast<ModelPart*>(index.internalPointer());
    QString text = selectedPart->data(0).toString();
    emit statusUpdateMessage(QString("The selected item is: ") + text, 0);
    selectionDetailsLabel->setText(selectedPart->summary());
    selectViewPart(selectedPart);

    int itemSliderValue = static_cast<int>(selectedPart->getScale() * 100);
    ui->itemScaleSlider->blockSignals(true);
    ui->itemScaleSlider->setValue(itemSliderValue);
    ui->itemScaleSlider->blockSignals(false);
}

void MainWindow::on_actionOpen_File_triggered() {
    QSettings settings("EngineeringGroup4", "VRModelViewer");
    const QString startDir = settings.value("lastStlDir", QDir::homePath()).toString();
    QString fileName = QFileDialog::getOpenFileName(
        this, tr("Open STL File"), startDir, tr("STL Files (*.stl)")
    );

    if (fileName.isEmpty()) {
        return;
    }

    settings.setValue("lastStlDir", QFileInfo(fileName).absolutePath());

    QModelIndex index = normalizedTreeIndex(ui->treeView->currentIndex());
    if (!index.isValid()) {
        QList<QVariant> newData;
        newData << QFileInfo(fileName).fileName() << "Visible";
        index = partList->appendChild(QModelIndex(), newData);
    }

    if (index.isValid()) {
        ModelPart* selectedPart = static_cast<ModelPart*>(index.internalPointer());
        selectedPart->set(0, QFileInfo(fileName).fileName());

        loadProgressBar->setRange(0, 0);
        loadProgressBar->setVisible(true);
        bulkLoading = true;
        selectedPart->loadSTL(fileName);
        emit partList->dataChanged(index, index.siblingAtColumn(1), { Qt::DisplayRole, Qt::ToolTipRole });
        bulkLoading = false;
        loadProgressBar->setVisible(false);
        loadProgressBar->setRange(0, 100);

        centerModelAtOrigin(index.parent());
        rebuildScene(true);
        updateSceneSummary();
        ui->treeView->setCurrentIndex(index);
        selectViewPart(selectedPart);
        selectionDetailsLabel->setText(selectedPart->summary());
        emit statusUpdateMessage(tr("Loaded %1").arg(QFileInfo(fileName).fileName()), 4000);
    }
}

void MainWindow::on_actionItemOptions_triggered()
{
    QModelIndex index = ui->treeView->currentIndex();
    if (!index.isValid()) {
        emit statusUpdateMessage("Warning: No item selected", 0);
        return;
    }

    QModelIndex cleanIndex = normalizedTreeIndex(index);
    ModelPart* selectedPart = static_cast<ModelPart*>(cleanIndex.internalPointer());

    Dialog dialog(this);
    dialog.setName(selectedPart->data(0).toString());
    dialog.setRGB(selectedPart->getColourR(), selectedPart->getColourG(), selectedPart->getColourB());
    dialog.setPartVisibility(selectedPart->visible());
    dialog.setShrinkEnabled(selectedPart->getShrinkEnabled());
    dialog.setClipEnabled(selectedPart->getClipEnabled());
    dialog.setFilterOrder(selectedPart->getFilterOrder());
    dialog.setGlowEnabled(selectedPart->glowEnabled());
    dialog.setGlowRGB(selectedPart->getGlowR(), selectedPart->getGlowG(), selectedPart->getGlowB());

    QList<ModelPart*> targetParts;
    QSet<ModelPart*> seenParts;
    const QString groupName = selectedPart->getGroupName();
    if (!groupName.isEmpty()) {
        for (ModelPart* part : getGroupMembers(QModelIndex(), groupName)) {
            if (part && !seenParts.contains(part)) {
                targetParts.append(part);
                seenParts.insert(part);
            }
        }
    }
    if (!seenParts.contains(selectedPart)) {
        targetParts.prepend(selectedPart);
        seenParts.insert(selectedPart);
    }

    /**
     * @struct PartOptionsSnapshot
     * @brief Captures editable part options so Cancel can restore the previous state.
     */
    struct PartOptionsSnapshot {
        ModelPart* part = nullptr; ///< Part whose state was captured.
        QString name;              ///< Original tree/display name.
        bool visible = true;       ///< Original visibility state.
        int r = 255;               ///< Original base red channel.
        int g = 255;               ///< Original base green channel.
        int b = 255;               ///< Original base blue channel.
        bool shrink = false;       ///< Original shrink filter state.
        bool clip = false;         ///< Original clip filter state.
        int filterOrder = 0;       ///< Original filter order.
        bool glow = false;         ///< Original glow enable state.
        int glowR = 0;             ///< Original glow red channel.
        int glowG = 180;           ///< Original glow green channel.
        int glowB = 255;           ///< Original glow blue channel.
    };

    auto channel = [](int value) {
        return static_cast<unsigned char>(std::clamp(value, 0, 255));
    };

    auto capture = [](ModelPart* part) {
        PartOptionsSnapshot snapshot;
        snapshot.part = part;
        snapshot.name = part->data(0).toString();
        snapshot.visible = part->visible();
        snapshot.r = part->getColourR();
        snapshot.g = part->getColourG();
        snapshot.b = part->getColourB();
        snapshot.shrink = part->getShrinkEnabled();
        snapshot.clip = part->getClipEnabled();
        snapshot.filterOrder = part->getFilterOrder();
        snapshot.glow = part->glowEnabled();
        snapshot.glowR = part->getGlowR();
        snapshot.glowG = part->getGlowG();
        snapshot.glowB = part->getGlowB();
        return snapshot;
    };

    QVector<PartOptionsSnapshot> snapshots;
    snapshots.reserve(targetParts.size());
    for (ModelPart* part : targetParts) {
        snapshots.append(capture(part));
    }

    auto emitPartChanged = [this](ModelPart* part) {
        QModelIndex partIndex = findIndexForPart(part);
        if (partIndex.isValid()) {
            emit partList->dataChanged(
                partIndex, partIndex.siblingAtColumn(1),
                { Qt::DisplayRole, Qt::EditRole, Qt::CheckStateRole,
                  Qt::ForegroundRole, Qt::BackgroundRole, Qt::ToolTipRole });
        }
    };

    auto pushVrPartUpdate = [this](ModelPart* part, int r, int g, int b,
                                   bool visible, bool glowEnabled,
                                   int glowR, int glowG, int glowB,
                                   bool geometryChanged) {
        if (!vrThread || !vrThread->isRunning()) {
            return;
        }
        vrThread->setActorAppearance(part->id(), r, g, b, visible, glowEnabled, glowR, glowG, glowB);
        if (geometryChanged) {
            vrThread->setActorGeometry(part->id(), part->createGeometrySnapshot());
        }
    };

    auto renderPreview = [this]() {
        if (renderWindow) {
            renderWindow->Render();
        }
    };

    auto applyOptions = [&](const QString& name, bool visible,
                            int r, int g, int b,
                            bool shrink, bool clip, int filterOrder,
                            bool glowEnabled, int glowR, int glowG, int glowB,
                            bool applyFilters,
                            bool refreshSummary,
                            bool updateAllTreeRows) {
        selectedPart->set(0, name);
        const QList<ModelPart*> partsToApply =
            (!applyFilters && !updateAllTreeRows) ? QList<ModelPart*>{ selectedPart } : targetParts;
        for (ModelPart* part : partsToApply) {
            bool geometryChanged = false;

            part->setColour(channel(r), channel(g), channel(b));
            part->setVisible(visible);
            part->setGlowEnabled(glowEnabled);
            part->setGlowColour(channel(glowR), channel(glowG), channel(glowB));

            if (applyFilters) {
                geometryChanged = part->getShrinkEnabled() != shrink
                    || part->getClipEnabled() != clip
                    || part->getFilterOrder() != filterOrder;

                part->setShrinkEnabled(shrink);
                part->setClipEnabled(clip);
                part->setFilterOrder(filterOrder);
                if (geometryChanged) {
                    part->updatePipeline();
                }
            }

            pushVrPartUpdate(part, r, g, b, visible, glowEnabled, glowR, glowG, glowB, geometryChanged);
            if (updateAllTreeRows || part == selectedPart) {
                emitPartChanged(part);
            }
        }

        selectionDetailsLabel->setText(selectedPart->summary());
        if (refreshSummary) {
            updateSceneSummary();
        }
        renderPreview();
    };

    auto restoreSnapshots = [&]() {
        for (const PartOptionsSnapshot& snapshot : snapshots) {
            if (!snapshot.part) {
                continue;
            }

            const bool geometryChanged = snapshot.part->getShrinkEnabled() != snapshot.shrink
                || snapshot.part->getClipEnabled() != snapshot.clip
                || snapshot.part->getFilterOrder() != snapshot.filterOrder;

            snapshot.part->set(0, snapshot.name);
            snapshot.part->setColour(channel(snapshot.r), channel(snapshot.g), channel(snapshot.b));
            snapshot.part->setVisible(snapshot.visible);
            snapshot.part->setShrinkEnabled(snapshot.shrink);
            snapshot.part->setClipEnabled(snapshot.clip);
            snapshot.part->setFilterOrder(snapshot.filterOrder);
            snapshot.part->setGlowEnabled(snapshot.glow);
            snapshot.part->setGlowColour(channel(snapshot.glowR), channel(snapshot.glowG), channel(snapshot.glowB));
            if (geometryChanged) {
                snapshot.part->updatePipeline();
            }

            pushVrPartUpdate(snapshot.part, snapshot.r, snapshot.g, snapshot.b,
                             snapshot.visible, snapshot.glow,
                             snapshot.glowR, snapshot.glowG, snapshot.glowB,
                             geometryChanged);
            emitPartChanged(snapshot.part);
        }
        selectionDetailsLabel->setText(selectedPart->summary());
        updateSceneSummary();
        renderPreview();
    };

    connect(&dialog, &Dialog::previewChanged, this,
            [&](const QString& name, bool visible,
                int r, int g, int b,
                bool shrink, bool clip, int filterOrder,
                bool glowEnabled, int glowR, int glowG, int glowB) {
        applyOptions(name, visible, r, g, b, shrink, clip, filterOrder,
                     glowEnabled, glowR, glowG, glowB,
                     false, false, false);
    });

    if (dialog.exec() == QDialog::Accepted) {
        int r = 0;
        int g = 0;
        int b = 0;
        int glowR = 0;
        int glowG = 0;
        int glowB = 0;
        dialog.getRGB(r, g, b);
        dialog.getGlowRGB(glowR, glowG, glowB);
        applyOptions(dialog.getName(), dialog.getVisible(),
                     r, g, b,
                     dialog.getShrinkEnabled(), dialog.getClipEnabled(), dialog.getFilterOrder(),
                     dialog.getGlowEnabled(), glowR, glowG, glowB,
                     true, true, true);
        emit statusUpdateMessage("Changes saved successfully.", 2500);
    }
    else {
        restoreSnapshots();
    }
}

void MainWindow::updateRender() {
    rebuildScene(false);
}

void MainWindow::updateRenderFromTree(const QModelIndex& index, QMap<QString, vtkSmartPointer<vtkAssembly>>& assemblies) {
    if (index.isValid()) {
        ModelPart* selectedPart = static_cast<ModelPart*>(index.internalPointer());
        vtkSmartPointer<vtkActor> actor = selectedPart->getActor();

        if (actor) {
            actorPartMap.insert(actor, selectedPart);
            if (propPicker) {
                propPicker->AddPickList(actor);
            }
            bool shouldShow = true;
            if (visibilityFilterOn) {
                shouldShow = selectedPart->visible();
            }
            actor->SetVisibility(shouldShow);

            actor->GetProperty()->SetColor(
                selectedPart->getColourR() / 255.0,
                selectedPart->getColourG() / 255.0,
                selectedPart->getColourB() / 255.0
            );

            QString groupName = selectedPart->getGroupName();

            if (!groupName.isEmpty()) {
                if (!assemblies.contains(groupName)) {
                    assemblies[groupName] = vtkSmartPointer<vtkAssembly>::New();
                }
                assemblies[groupName]->AddPart(actor);
            }
            else {
                renderer->AddActor(actor);
            }
        }
    }

    if (partList->hasChildren(index)) {
        for (int i = 0; i < partList->rowCount(index); i++) {
            updateRenderFromTree(partList->index(i, 0, index), assemblies);
        }
    }
}

void MainWindow::on_actionAdd_Level_triggered() {
    QList<QVariant> newData;
    newData << "New Top Level" << "Visible";

    QModelIndex emptyIndex = QModelIndex();
    QModelIndex newLevelIndex = partList->appendChild(emptyIndex, newData);

    if (newLevelIndex.isValid()) {
        ModelPart* newPart = static_cast<ModelPart*>(newLevelIndex.internalPointer());
        newPart->setColour(255, 255, 255);
        newPart->setVisible(true);

        ui->treeView->setCurrentIndex(newLevelIndex);
        updateRender();
    }
}

void MainWindow::on_actionAdd_Item_triggered() {
    QModelIndex index = normalizedTreeIndex(ui->treeView->currentIndex());
    if (!index.isValid()) {
        emit statusUpdateMessage("Please select a parent Level/Item first.", 0);
        return;
    }

    QList<QVariant> newData;
    newData << "New Sub-Item" << "Visible";

    QModelIndex newChildIndex = partList->appendChild(index, newData);

    if (newChildIndex.isValid()) {
        ModelPart* newPart = static_cast<ModelPart*>(newChildIndex.internalPointer());
        newPart->setColour(255, 255, 255);
        newPart->setVisible(true);

        ui->treeView->expand(index);
        ui->treeView->setCurrentIndex(newChildIndex);
        updateRender();
    }
}

void MainWindow::on_actionDelete_Item_triggered() {
    QModelIndex index = ui->treeView->currentIndex();
    if (!index.isValid()) {
        emit statusUpdateMessage("Warning: No item selected for deletion", 0);
        return;
    }

    QModelIndex cleanIndex = normalizedTreeIndex(index);
    ModelPart* selectedPart = static_cast<ModelPart*>(cleanIndex.internalPointer());
    QString name = selectedPart->data(0).toString();

    auto reply = QMessageBox::question(this, "Delete Confirmation",
        QString("Are you sure you want to delete '%1'?").arg(name),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        undoStack->clear();
        if (partList->removePart(cleanIndex)) {
            if (selectedViewPart == selectedPart) {
                selectedViewPart = nullptr;
                syncTransformControls(nullptr);
            }
            if (hoveredPart == selectedPart) {
                hoveredPart = nullptr;
            }
            selectionDetailsLabel->setText(tr("No part selected"));
            updateRender();
            updateSceneSummary();
            emit statusUpdateMessage(QString("Item '%1' deleted.").arg(name), 0);
        }
    }
}

void MainWindow::on_actionStart_VR_triggered() {
    int loadedParts = 0;
    int visibleParts = 0;
    qint64 triangles = 0;
    collectSceneStats(QModelIndex(), loadedParts, visibleParts, triangles);
    if (loadedParts == 0) {
        QMessageBox::information(this, tr("No Model Loaded"), tr("Load at least one STL part before starting VR."));
        return;
    }

    if (vrThread && vrThread->isRunning()) {
        emit statusUpdateMessage(tr("VR is already running."), 3000);
        return;
    }

    vr::EVRInitError eError = vr::VRInitError_None;
    vr::VR_Init(&eError, vr::VRApplication_Scene);

    if (eError != vr::VRInitError_None) {
        QMessageBox::critical(this, "VR Initialization Failed",
            QString("Failed to detect VR equipment or SteamVR is not running.\nError: %1")
            .arg(vr::VR_GetVRInitErrorAsEnglishDescription(eError)));
        return;
    }
    vr::VR_Shutdown();

    vrThread = new VRRenderThread();
    connect(vrThread, &VRRenderThread::actorTransformChanged,
            this, &MainWindow::applyVrTransformUpdate,
            Qt::QueuedConnection);
    connect(vrThread, &VRRenderThread::actorColorChanged,
            this, &MainWindow::applyVrColorUpdate,
            Qt::QueuedConnection);
    double bounds[6];
    if (collectModelBounds(QModelIndex(), bounds)) {
        vrThread->setSceneBounds(bounds);
    }
    vrThread->setViewTuning(
        vrModelSizeSpin->value(),
        vrDistanceSpin->value(),
        vrHeightOffsetSpin->value());
    vrThread->setEnvironmentOrientation(
        hdrTiltSpin ? hdrTiltSpin->value() : 0.0,
        hdrTiltYSpin ? hdrTiltYSpin->value() : 0.0,
        hdrHeadingSpin ? hdrHeadingSpin->value() : 0.0);
    vrThread->setEnvironmentPath(hdrComboBox ? hdrComboBox->currentData().toString() : QString());
    if (showcaseSpinAction && showcaseSpinAction->isChecked()) {
        vrThread->issueCommand(VRRenderThread::SET_SHOWCASE_SPIN, 1.0);
    }
    vrThread->setViewMode(static_cast<VRRenderThread::ViewMode>(vrViewModeCombo->currentData().toInt()));
    vrThread->setViewYawDegrees(vrViewAngleCombo->currentData().toDouble());
    collectActorsForVR(QModelIndex());
    vrThread->start();

    emit statusUpdateMessage("VR system ready.", 2000);
    ui->actionStart_VR->setVisible(false);
    ui->actionPause_VR->setVisible(true);
    ui->actionStop_VR->setVisible(true);
    ui->actionPause_VR->setText("Pause VR");
}

void MainWindow::collectActorsForVR(const QModelIndex& index) {
    if (index.isValid()) {
        ModelPart* part = static_cast<ModelPart*>(index.internalPointer());
        vtkActor* vrActor = part->getNewActor();

        if (vrActor) {
            vrThread->addActorOffline(part->id(), vrActor);
            part->detachVrActor();
        }
    }

    if (partList->hasChildren(index)) {
        for (int i = 0; i < partList->rowCount(index); i++) {
            collectActorsForVR(partList->index(i, 0, index));
        }
    }
}

void MainWindow::on_actionToggle_Filter_triggered()
{
    visibilityFilterOn = ui->actionToggle_Filter->isChecked();

    updateRender();

    emit statusUpdateMessage(
        QString("Visibility filter is now %1")
        .arg(visibilityFilterOn ? "ON - showing checked parts only" : "OFF - showing all loaded parts"),
        3000
    );
}

void MainWindow::on_zoomSlider_valueChanged(int value) {
    if (!renderer) return;

    double zoomFactor = value / 100.0;
    if (zoomFactor <= 0) zoomFactor = 0.01;

    vtkCamera* camera = renderer->GetActiveCamera();
    camera->SetViewAngle(30.0 / zoomFactor);
    renderer->ResetCameraClippingRange();
    renderWindow->Render();
}

void MainWindow::on_lightSlider_valueChanged(int value)
{
    double brightness = value / 100.0;
    vtkLightCollection* lights = renderer->GetLights();
    lights->InitTraversal();
    vtkLight* myLight = lights->GetNextItem();

    if (myLight != nullptr) {
        myLight->SetIntensity(brightness);
        renderWindow->Render();
    }
}

void MainWindow::on_actionOpen_Folder_triggered() {
    QSettings settings("EngineeringGroup4", "VRModelViewer");
    const QString startDir = settings.value("lastStlDir", QDir::homePath()).toString();
    QString dirPath = QFileDialog::getExistingDirectory(
        this, tr("Open STL Folder"), startDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (dirPath.isEmpty()) return;
    settings.setValue("lastStlDir", dirPath);

    QFileInfoList fileList;
    QDirIterator iterator(
        dirPath,
        QStringList() << "*.stl" << "*.STL",
        QDir::Files | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);

    while (iterator.hasNext()) {
        fileList.append(QFileInfo(iterator.next()));
    }

    std::sort(fileList.begin(), fileList.end(), [](const QFileInfo& left, const QFileInfo& right) {
        return QString::localeAwareCompare(left.absoluteFilePath(), right.absoluteFilePath()) < 0;
        });

    if (fileList.isEmpty()) {
        emit statusUpdateMessage("Warning: No STL files found in folder.", 3000);
        return;
    }

    QModelIndex parentIndex = normalizedTreeIndex(ui->treeView->currentIndex());
    bool createdFolderRoot = false;
    if (!parentIndex.isValid()) {
        QList<QVariant> folderData;
        const QString folderName = QDir(dirPath).dirName().isEmpty() ? tr("Imported STL Assembly") : QDir(dirPath).dirName();
        folderData << folderName << "Visible";
        parentIndex = partList->appendChild(QModelIndex(), folderData);
        createdFolderRoot = parentIndex.isValid();
    }

    QProgressDialog progress(tr("Loading STL assembly..."), tr("Cancel"), 0, fileList.size(), this);
    progress.setWindowTitle(tr("Loading Folder"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    loadProgressBar->setRange(0, fileList.size());
    loadProgressBar->setValue(0);
    loadProgressBar->setVisible(true);

    bulkLoading = true;
    ui->treeView->setUpdatesEnabled(false);
    QElapsedTimer loadTimer;
    loadTimer.start();

    int loadedCount = 0;
    for (const QFileInfo& fileInfo : fileList) {
        if (progress.wasCanceled()) {
            break;
        }

        QList<QVariant> newData;
        newData << fileInfo.fileName() << "Visible";
        QModelIndex newChildIndex = partList->appendChild(parentIndex, newData);
        if (newChildIndex.isValid()) {
            ModelPart* newPart = static_cast<ModelPart*>(newChildIndex.internalPointer());
            newPart->loadSTL(fileInfo.absoluteFilePath());
            loadedCount++;
        }

        progress.setValue(loadedCount);
        loadProgressBar->setValue(loadedCount);
        if (loadedCount % 25 == 0) {
            QCoreApplication::processEvents();
        }
    }

    bulkLoading = false;
    ui->treeView->setUpdatesEnabled(true);
    progress.setValue(fileList.size());
    loadProgressBar->setVisible(false);

    if (createdFolderRoot && loadedCount == 0) {
        partList->removePart(parentIndex);
        updateSceneSummary();
        return;
    }

    if (parentIndex.isValid()) {
        centerModelAtOrigin(parentIndex);
        ui->treeView->expand(parentIndex);
        ui->treeView->setCurrentIndex(parentIndex);
    }

    emit partList->layoutChanged();
    rebuildScene(true);
    updateSceneSummary();

    const QString message = progress.wasCanceled()
        ? tr("Loaded %1 of %2 STL files before cancellation.").arg(loadedCount).arg(fileList.size())
        : tr("Loaded %1 STL files in %2 seconds.").arg(loadedCount).arg(loadTimer.elapsed() / 1000.0, 0, 'f', 1);
    emit statusUpdateMessage(message, 5000);
}

void MainWindow::on_itemScaleSlider_valueChanged(int value) {
    QModelIndex index = normalizedTreeIndex(ui->treeView->currentIndex());
    if (!index.isValid()) return;

    ModelPart* selectedPart = static_cast<ModelPart*>(index.internalPointer());

    double newScale = value / 100.0;
    QString groupName = selectedPart->getGroupName();

    if (!groupName.isEmpty()) {
        QList<ModelPart*> groupParts = getGroupMembers(QModelIndex(), groupName);
        for (ModelPart* part : groupParts) {
            part->setScale(newScale);
        }
    }
    else {
        selectedPart->setScale(newScale);
    }

    syncTransformControls(selectedPart);
    renderWindow->Render();
}

void MainWindow::on_actionStop_VR_triggered() {
    stopVRThread();
    ui->actionStart_VR->setVisible(true);
    ui->actionPause_VR->setVisible(false);
    ui->actionStop_VR->setVisible(false);
    emit statusUpdateMessage(tr("VR stopped."), 3000);
}

void MainWindow::on_actionPause_VR_triggered() {
    if (vrThread != nullptr && vrThread->isRunning()) {
        const bool pauseNow = (ui->actionPause_VR->text() == tr("Pause VR"));
        vrThread->issueCommand(VRRenderThread::PAUSE_RENDER, pauseNow ? 1.0 : 0.0);
        ui->actionPause_VR->setText(pauseNow ? tr("Resume VR") : tr("Pause VR"));
    }
}

void MainWindow::on_actionLock_to_New_Group_triggered() {

    QModelIndexList selectedIndexes = ui->treeView->selectionModel()->selectedRows();
    if (selectedIndexes.isEmpty()) return;

    static int groupCounter = 1;

    QString newGroupName = QString("Group %1").arg(groupCounter++);

    QTreeWidgetItem* groupItem = new QTreeWidgetItem(ui->groupTreeWidget);
    groupItem->setText(0, newGroupName);
    groupItem->setBackground(0, QBrush(QColor(220, 220, 220)));

    for (const QModelIndex& index : selectedIndexes) {
        QModelIndex cleanIndex = normalizedTreeIndex(index);
        ModelPart* selectedPart = static_cast<ModelPart*>(cleanIndex.internalPointer());

        if (selectedPart->isLockedInGroup()) continue;

        QTreeWidgetItem* childItem = new QTreeWidgetItem(groupItem);
        childItem->setText(0, selectedPart->data(0).toString());

        QVariant pointerData = QVariant::fromValue(reinterpret_cast<void*>(selectedPart));
        childItem->setData(0, Qt::UserRole, pointerData);

        selectedPart->setGroupName(newGroupName);
    }

    if (groupItem->childCount() == 0) {
        delete groupItem;
        return;
    }

    ui->groupTreeWidget->expandItem(groupItem);
    emit statusUpdateMessage(QString("Successfully locked %1 items to %2").arg(groupItem->childCount()).arg(newGroupName), 3000);
}

void MainWindow::cleanupEmptyGroups() {
    bool needsFullUpdate = false;

    for (int i = ui->groupTreeWidget->topLevelItemCount() - 1; i >= 0; --i) {
        QTreeWidgetItem* topItem = ui->groupTreeWidget->topLevelItem(i);
        QVariant data = topItem->data(0, Qt::UserRole);

        if (!data.isValid()) {
            if (topItem->childCount() == 0) {
                delete topItem;
                needsFullUpdate = true;
            }
            else {
                QString currentGroupName = topItem->text(0);

                unsigned char masterR = 255, masterG = 255, masterB = 255;
                double masterScale = 1.0;

                QTreeWidgetItem* firstChild = topItem->child(0);
                QVariant firstData = firstChild->data(0, Qt::UserRole);
                if (firstData.isValid()) {
                    ModelPart* firstPart = static_cast<ModelPart*>(firstData.value<void*>());
                    if (firstPart) {
                        masterR = firstPart->getColourR();
                        masterG = firstPart->getColourG();
                        masterB = firstPart->getColourB();
                        masterScale = firstPart->getScale();
                    }
                }

                for (int j = 0; j < topItem->childCount(); ++j) {
                    QTreeWidgetItem* childItem = topItem->child(j);
                    QVariant childData = childItem->data(0, Qt::UserRole);
                    if (childData.isValid()) {
                        ModelPart* part = static_cast<ModelPart*>(childData.value<void*>());
                        if (part) {
                            if (part->getGroupName() != currentGroupName) {
                                part->setGroupName(currentGroupName);
                                needsFullUpdate = true;
                            }

                            if (part->getColourR() != masterR || part->getColourG() != masterG || part->getColourB() != masterB) {
                                part->setColour(masterR, masterG, masterB);
                                needsFullUpdate = true;
                            }

                            if (std::abs(part->getScale() - masterScale) > 0.001) {
                                part->setScale(masterScale);
                                needsFullUpdate = true;
                            }
                        }
                    }
                }
            }
        }
        else {
            ModelPart* part = static_cast<ModelPart*>(data.value<void*>());
            if (part) {
                part->setGroupName("");
                needsFullUpdate = true;
            }
            delete topItem;
        }
    }

    if (needsFullUpdate) {
        updateRender();
    }
}

void MainWindow::on_actionUnlock_Item_triggered() {
    QTreeWidgetItem* selectedItem = ui->groupTreeWidget->currentItem();
    if (!selectedItem) return;

    if (selectedItem->parent()) {
        QVariant data = selectedItem->data(0, Qt::UserRole);
        if (data.isValid()) {
            ModelPart* part = static_cast<ModelPart*>(data.value<void*>());
            part->setGroupName("");
        }
        delete selectedItem;
        cleanupEmptyGroups();
    }
    else {
        // Dissolve entire group
        while (selectedItem->childCount() > 0) {
            QTreeWidgetItem* child = selectedItem->child(0);
            QVariant data = child->data(0, Qt::UserRole);
            if (data.isValid()) {
                ModelPart* part = static_cast<ModelPart*>(data.value<void*>());
                part->setGroupName("");
            }
            delete child;
        }
        delete selectedItem;
        updateRender();
    }
}

QList<ModelPart*> MainWindow::getGroupMembers(const QModelIndex& parentIndex, const QString& groupName) {
    QList<ModelPart*> members;
    if (groupName.isEmpty()) return members;

    int rowCount = partList->rowCount(parentIndex);
    for (int i = 0; i < rowCount; ++i) {
        QModelIndex childIndex = partList->index(i, 0, parentIndex);
        ModelPart* part = static_cast<ModelPart*>(childIndex.internalPointer());

        if (part->getGroupName() == groupName) {
            members.append(part);
        }

        if (partList->hasChildren(childIndex)) {
            members.append(getGroupMembers(childIndex, groupName));
        }
    }
    return members;
}
