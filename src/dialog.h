/**
 * @file dialog.h
 * @brief Declares the Part Options dialog used to edit STL part appearance and filters.
 *
 * The dialog exposes colour, visibility, glow, and filter settings for a selected
 * ModelPart. It emits a lightweight preview signal so the main window can update
 * the renderer while the user is choosing appearance options.
 */

#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <QColorDialog>
#include <QColor>

class QCheckBox;
class QPushButton;

namespace Ui {
class Dialog;
}

/**
 * @class Dialog
 * @brief Modal Qt dialog for editing a part's display options.
 *
 * Dialog owns the widgets created from dialog.ui and adds a small set of custom
 * controls for glow highlighting. Getter and setter methods are used by MainWindow
 * to initialise the dialog from the selected part and read the final accepted state.
 */
class Dialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Creates the part options dialog.
     * @param parent Optional parent widget used for Qt ownership and modality.
     */
    explicit Dialog(QWidget *parent = nullptr);

    /**
     * @brief Destroys the dialog and releases the generated UI object.
     */
    ~Dialog();

    /**
     * @brief Sets the editable part name field.
     * @param name Name to show in the dialog.
     */
    void setName(const QString& name);

    /**
     * @brief Sets the visible checkbox state.
     * @param v True if the part should be visible.
     */
    void setPartVisibility(bool v);

    /**
     * @brief Sets the selected base colour.
     * @param r Red channel in the range 0-255.
     * @param g Green channel in the range 0-255.
     * @param b Blue channel in the range 0-255.
     */
    void setRGB(int r, int g, int b);

    /**
     * @brief Reads the current part name from the dialog.
     * @return Name currently typed by the user.
     */
    QString getName() const;

    /**
     * @brief Reads the visible checkbox state.
     * @return True when the part should be visible.
     */
    bool getVisible() const;

    /**
     * @brief Reads the selected base colour.
     * @param r Output red channel in the range 0-255.
     * @param g Output green channel in the range 0-255.
     * @param b Output blue channel in the range 0-255.
     */
    void getRGB(int& r, int& g, int& b) const;

    /**
     * @brief Sets whether the shrink filter checkbox is enabled.
     * @param enabled True to request the shrink filter.
     */
    void setShrinkEnabled(bool enabled);

    /**
     * @brief Sets whether the clip filter checkbox is enabled.
     * @param enabled True to request the clip filter.
     */
    void setClipEnabled(bool enabled);

    /**
     * @brief Sets the selected filter order.
     * @param order Combo-box index, where 0 is shrink-then-clip and 1 is clip-then-shrink.
     */
    void setFilterOrder(int order);

    /**
     * @brief Sets whether glow highlighting is enabled.
     * @param enabled True to enable glow highlighting.
     */
    void setGlowEnabled(bool enabled);

    /**
     * @brief Sets the glow highlight colour.
     * @param r Red channel in the range 0-255.
     * @param g Green channel in the range 0-255.
     * @param b Blue channel in the range 0-255.
     */
    void setGlowRGB(int r, int g, int b);

    /**
     * @brief Reads the shrink filter checkbox.
     * @return True if the shrink filter should be applied.
     */
    bool getShrinkEnabled() const;

    /**
     * @brief Reads the clip filter checkbox.
     * @return True if the clip filter should be applied.
     */
    bool getClipEnabled() const;

    /**
     * @brief Reads the chosen filter order.
     * @return Combo-box index for filter ordering.
     */
    int getFilterOrder() const;

    /**
     * @brief Reads the glow checkbox state.
     * @return True if glow highlighting should be applied.
     */
    bool getGlowEnabled() const;

    /**
     * @brief Reads the selected glow highlight colour.
     * @param r Output red channel in the range 0-255.
     * @param g Output green channel in the range 0-255.
     * @param b Output blue channel in the range 0-255.
     */
    void getGlowRGB(int& r, int& g, int& b) const;

signals:
    /**
     * @brief Emitted when lightweight preview properties change.
     * @param name Current part name.
     * @param visible Current visibility state.
     * @param r Base colour red channel.
     * @param g Base colour green channel.
     * @param b Base colour blue channel.
     * @param shrinkEnabled Current shrink filter state.
     * @param clipEnabled Current clip filter state.
     * @param filterOrder Current filter order index.
     * @param glowEnabled Current glow state.
     * @param glowR Glow colour red channel.
     * @param glowG Glow colour green channel.
     * @param glowB Glow colour blue channel.
     */
    void previewChanged(const QString& name, bool visible,
                        int r, int g, int b,
                        bool shrinkEnabled, bool clipEnabled, int filterOrder,
                        bool glowEnabled, int glowR, int glowG, int glowB);

private slots:
    /**
     * @brief Opens the colour picker for the base part colour.
     */
    void on_pushButton_clicked();

    /**
     * @brief Opens the colour picker for the glow colour.
     */
    void onGlowColorClicked();

private:
    Ui::Dialog *ui;                         ///< Generated widget tree from dialog.ui.
    QColor selectedColor;                   ///< Currently selected base part colour.
    QColor selectedGlowColor = QColor(0, 180, 255); ///< Currently selected glow highlight colour.
    QCheckBox* glowCheckBox = nullptr;      ///< Runtime-created checkbox for glow enable state.
    QPushButton* glowColorButton = nullptr; ///< Runtime-created button used to choose glow colour.

    /**
     * @brief Updates a colour button's swatch and readable foreground colour.
     * @param button Button whose stylesheet should be updated.
     * @param color Colour represented by the button.
     */
    void updateColorButton(QPushButton* button, const QColor& color);

    /**
     * @brief Emits previewChanged with the dialog's current lightweight state.
     */
    void emitPreviewChanged();
};

#endif // DIALOG_H
