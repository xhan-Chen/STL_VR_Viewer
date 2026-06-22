#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <QString>

namespace Ui {
class Dialog;
}

class Dialog : public QDialog
{
    Q_OBJECT

public:
    explicit Dialog(QWidget *parent = nullptr);
    ~Dialog();

    void setName(const QString &name);
    void setRGB(int r, int g, int b);


    QString getName() const;
    void getRGB(int &r, int &g, int &b) const;
bool getVisible() const;
private:
    Ui::Dialog *ui;
};

#endif // DIALOG_H
