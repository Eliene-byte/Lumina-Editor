// ValueEditors.h - Um editor por tipo de valor.
//
// Ficam em arquivos proprios porque sao a maior parte do codigo de UI do
// projeto e nao tem dependencia com o resto: qualquer um pode ser testado
// isoladamente.
#pragma once

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "core/Property.h"
#include "core/Value.h"

namespace lmn::ui {

// --- Numerico: slider + spinbox no mesmo controle --------------------------
// O duplo controle e proposital: o slider para ajuste fino rapido, o campo
// para valor exato e digitacao. E o que a maioria dos editores sérios faz.
class NumberEditor : public QWidget, public ValueEditor {
    Q_OBJECT
public:
    NumberEditor(QWidget* parent, double value, double lo, double hi, int decimals,
                 double step);

    QWidget* widget() override { return this; }
    void setValueQuiet(const Value& v) override;
    Value value() const override;
    void setRange(double lo, double hi);
    void setHasKeyframe(bool on) { m_hasKey = on; }
    // Slider = arrastar rapido; spinbox = digitar.
    void setUseSliderOnly(bool only) { m_sliderOnly = only; }
    [[nodiscard]] double rangeMin() const { return m_lo; }
    [[nodiscard]] double rangeMax() const { return m_hi; }

signals:
    void valueChanged(double value);
    void editingFinished();

private:
    void syncFromSlider();
    void syncFromSpin();

    QSlider* m_slider = nullptr;
    QDoubleSpinBox* m_spin = nullptr;
    double m_lo = 0.0;
    double m_hi = 1.0;
    int m_decimals = 3;
    double m_step = 0.01;
    bool m_updating = false;
    bool m_sliderOnly = false;
    bool m_hasKey = false;
};

// --- Cor -------------------------------------------------------------------
class ColorEditor : public QWidget, public ValueEditor {
    Q_OBJECT
public:
    explicit ColorEditor(QWidget* parent, const Color& value);

    QWidget* widget() override { return this; }
    void setValueQuiet(const Value& v) override;
    Value value() const override;

    // Abre o seletor com alfa habilitado.
    void chooseColor();
    void setHasKeyframe(bool on) { m_hasKey = on; }

signals:
    void valueChanged(Color color);
    void editingFinished();

private:
    void updateSwatch();
    Color m_color;
    QPushButton* m_swatch = nullptr;
    QLabel* m_label = nullptr;
    bool m_updating = false;
    bool m_hasKey = false;
};

// --- Texto -----------------------------------------------------------------
class TextEditor : public QLineEdit, public ValueEditor {
    Q_OBJECT
public:
    TextEditor(QWidget* parent, const std::string& value, bool isFilePath);

    QWidget* widget() override { return this; }
    void setValueQuiet(const Value& v) override;
    Value value() const override;

    void chooseFile();
    void setHasKeyframe(bool on);

signals:
    void valueChanged(QString text);

private:
    bool m_isFilePath = false;
    QPushButton* m_browse = nullptr;
    QWidget* m_holder = nullptr;
    QHBoxLayout* m_layout = nullptr;
    bool m_updating = false;
};

// --- Lista (Enum) ----------------------------------------------------------
class EnumEditor : public QComboBox, public ValueEditor {
    Q_OBJECT
public:
    EnumEditor(QWidget* parent, const std::string& value,
               const std::vector<std::string>& options);

    QWidget* widget() override { return this; }
    void setValueQuiet(const Value& v) override;
    Value value() const override;
    void setHasKeyframe(bool on);

signals:
    void valueChanged(QString value);

private:
    bool m_updating = false;
};

// --- Vetor (posicao, escala) ------------------------------------------------
// Dois campos com o rotulo X e Y. A Property guarda um Vec2, o que faz a
// animacao de X e Y ser independente - como o usuario espera.
class VectorEditor : public QWidget, public ValueEditor {
    Q_OBJECT
public:
    VectorEditor(QWidget* parent, const Vec2& value, double lo, double hi,
                 int decimals);
    VectorEditor(QWidget* parent, const Vec3& value, double lo, double hi,
                 int decimals);

    QWidget* widget() override { return this; }
    void setValueQuiet(const Value& v) override;
    Value value() const override;
    void setHasKeyframe(bool on);
    // O eixo Y do modelo sobe, o da tela desce.
    void setInvertY(bool on) { m_invertY = on; }

signals:
    void valueChanged(double x, double y, double z);
    void editingFinished();

private:
    QDoubleSpinBox* m_fields[3] = {nullptr, nullptr, nullptr};
    int m_count = 2;
    double m_lo, m_hi;
    int m_decimals;
    bool m_updating = false;
    bool m_invertY = false;
};

// --- Booleano --------------------------------------------------------------
class BoolEditor : public QCheckBox, public ValueEditor {
    Q_OBJECT
public:
    explicit BoolEditor(QWidget* parent, bool value);

    QWidget* widget() override { return this; }
    void setValueQuiet(const Value& v) override;
    Value value() const override;
    void setHasKeyframe(bool on);

signals:
    void valueChanged(bool value);

private:
    bool m_updating = false;
};

// --- Angulo ----------------------------------------------------------------
// Graus com a possibilidade de digitar "90+45" ou expressoes, que e como quem
// anima thinks em rotacao.
class AngleEditor : public NumberEditor {
    Q_OBJECT
public:
    AngleEditor(QWidget* parent, double degrees)
        : NumberEditor(parent, degrees, -100000, 100000, 2, 1.0) {}

    void setValueQuiet(const Value& v) override;
};

}  // namespace lmn::ui
