#include "dialog.h"
#include "ui_dialog.h"

Dialog::Dialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Dialog)
{
    ui->setupUi(this);
    this->setWindowTitle("Item Options");
}

Dialog::~Dialog()
{
    delete ui;
}

// --- Setters ---
void Dialog::setName(const QString &name) {
    if (ui->nameEdit) ui->nameEdit->setText(name);
}



void Dialog::setRGB(int r, int g, int b) {
    if (ui->rSpinBox) ui->rSpinBox->setValue(r);
    if (ui->gSpinBox) ui->gSpinBox->setValue(g);
    if (ui->bSpinBox) ui->bSpinBox->setValue(b);
}

// --- Getters ---
QString Dialog::getName() const {
    return ui->nameEdit ? ui->nameEdit->text() : QString();
}

// 在 dialog.cpp 中
bool Dialog::getVisible() const {
    return ui->visibleCheckBox->isChecked();
}

void Dialog::getRGB(int &r, int &g, int &b) const {
    r = ui->rSpinBox->value();
    g = ui->gSpinBox->value();
    b = ui->bSpinBox->value();
}
