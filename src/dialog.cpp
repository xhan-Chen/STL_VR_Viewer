/**
 * @file dialog.cpp
 * @brief Implements the Part Options dialog used to edit colour, visibility, glow, and filters.
 */

#include "dialog.h"
#include "ui_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

Dialog::Dialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Dialog)
{
    ui->setupUi(this);
    this->setWindowTitle("Part Options");
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    glowCheckBox = new QCheckBox(tr("Enable glow highlight"), this);
    glowColorButton = new QPushButton(tr("Glow Color"), this);
    ui->verticalLayout->insertWidget(ui->verticalLayout->count() - 2, glowCheckBox);
    ui->verticalLayout->insertWidget(ui->verticalLayout->count() - 2, glowColorButton);
    connect(glowColorButton, &QPushButton::clicked, this, &Dialog::onGlowColorClicked);
    connect(ui->nameEdit, &QLineEdit::textChanged, this, &Dialog::emitPreviewChanged);
    connect(ui->visibleCheckBox, &QCheckBox::toggled, this, &Dialog::emitPreviewChanged);
    connect(glowCheckBox, &QCheckBox::toggled, this, &Dialog::emitPreviewChanged);

    setMinimumWidth(420);
    setStyleSheet(QStringLiteral(
        "QDialog { background: #111827; color: #e5e7eb; font-family: 'Segoe UI Variable', 'Segoe UI'; }"
        "QLineEdit, QComboBox { background: #1f2937; color: #f9fafb; border: 1px solid #374151; border-radius: 6px; padding: 6px; }"
        "QCheckBox { padding: 5px 0; }"
        "QLabel { color: #9ca3af; font-weight: 600; }"
        "QPushButton { background: #1f2937; color: #f9fafb; border: 1px solid #4b5563; border-radius: 6px; padding: 7px 10px; }"
        "QPushButton:hover { background: #26364d; border-color: #60a5fa; }"
        "QDialogButtonBox QPushButton { min-width: 78px; }"
    ));
}

Dialog::~Dialog()
{
    delete ui;
}

// --- 1. SETTERS (Fill the UI when the window opens) ---
void Dialog::setName(const QString &name) {
    ui->nameEdit->setText(name);
}

void Dialog::setPartVisibility(bool v) {
    ui->visibleCheckBox->setChecked(v);
}

void Dialog::setRGB(int r, int g, int b) {
    selectedColor = QColor(r, g, b);
    updateColorButton(ui->pushButton, selectedColor);
}

// --- 2. GETTERS (Read the UI when the window closes) ---
QString Dialog::getName() const {
    return ui->nameEdit->text();
}

bool Dialog::getVisible() const {
    return ui->visibleCheckBox->isChecked();
}

void Dialog::getRGB(int &r, int &g, int &b) const {
    r = selectedColor.red();
    g = selectedColor.green();
    b = selectedColor.blue();
}

// --- 3. COLOR PICKER LOGIC ---
void Dialog::on_pushButton_clicked()
{
    // Pop open the color wheel!
    QColor color = QColorDialog::getColor(selectedColor, this, "Select Part Color");

    if (color.isValid()) {
        selectedColor = color;
        updateColorButton(ui->pushButton, selectedColor);
        emitPreviewChanged();
    }
}

void Dialog::onGlowColorClicked()
{
    QColor color = QColorDialog::getColor(selectedGlowColor, this, "Select Glow Color");
    if (color.isValid()) {
        selectedGlowColor = color;
        updateColorButton(glowColorButton, selectedGlowColor);
        emitPreviewChanged();
    }
}

void Dialog::setShrinkEnabled(bool enabled)
{
    ui->shrinkCheckBox->setChecked(enabled);
}

void Dialog::setClipEnabled(bool enabled)
{
    ui->clipCheckBox->setChecked(enabled);
}

void Dialog::setFilterOrder(int order)
{
    ui->comboBox->setCurrentIndex(order);
}

bool Dialog::getShrinkEnabled() const
{
    return ui->shrinkCheckBox->isChecked();
}

bool Dialog::getClipEnabled() const
{
    return ui->clipCheckBox->isChecked();
}

int Dialog::getFilterOrder() const
{
    return ui->comboBox->currentIndex();
}

void Dialog::setGlowEnabled(bool enabled)
{
    if (glowCheckBox) {
        glowCheckBox->setChecked(enabled);
    }
}

void Dialog::setGlowRGB(int r, int g, int b)
{
    selectedGlowColor = QColor(r, g, b);
    if (glowColorButton) {
        updateColorButton(glowColorButton, selectedGlowColor);
    }
}

bool Dialog::getGlowEnabled() const
{
    return glowCheckBox && glowCheckBox->isChecked();
}

void Dialog::getGlowRGB(int& r, int& g, int& b) const
{
    r = selectedGlowColor.red();
    g = selectedGlowColor.green();
    b = selectedGlowColor.blue();
}

void Dialog::updateColorButton(QPushButton* button, const QColor& color)
{
    if (!button) return;
    const QString textColor = color.lightness() < 128 ? "white" : "#111827";
    button->setStyleSheet(QString("background-color: %1; color: %2; border-color: %1;")
        .arg(color.name(), textColor));

}

void Dialog::emitPreviewChanged()
{
    int r = 0;
    int g = 0;
    int b = 0;
    int glowR = 0;
    int glowG = 0;
    int glowB = 0;
    getRGB(r, g, b);
    getGlowRGB(glowR, glowG, glowB);
    emit previewChanged(getName(), getVisible(),
                        r, g, b,
                        getShrinkEnabled(), getClipEnabled(), getFilterOrder(),
                        getGlowEnabled(), glowR, glowG, glowB);
}

