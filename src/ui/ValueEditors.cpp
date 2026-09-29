#include "ValueEditors.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QPainter>
#include <QVBoxLayout>

#include <cmath>

namespace lmn::ui {
namespace {

QString formatColor(const Color& c) {
    return QStringLiteral("%1  %2  %3")
        .arg(c.r, 0, 'f', 3).arg(c.g, 0, 'f', 3).arg(c.b, 0, 'f', 3);
}

QColor toQColor(const Color& c) {
    // Os valores estao em linear; a cor de tela precisa de gamma.
    auto encode = [](double v) {
        v = std::clamp(v, 0.0, 1.0);
        const double s = v <= 0.0031308 ? v * 12.92
                                         : 1.055 * std::pow(v, 1.0 / 2.4) - 0.055;
        return static_cast<int>(std::lround(s * 255.0));
    };
    return QColor(encode(c.r), encode(c.g), encode(c.b), encode(c.a));
}

Color fromQColor(const QColor& q) {
    auto decode = [](int v) {
        const double s = v / 255.0;
        return s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
    };
    return Color(decode(q.red()), decode(q.green()), decode(q.blue()),
                 decode(q.alpha()));
}

}  // namespace

// ---------------------------------------------------------------------------
// NumberEditor
// ---------------------------------------------------------------------------

NumberEditor::NumberEditor(QWidget* parent, double value, double lo, double hi,
                           int decimals, double step)
    : QWidget(parent), m_lo(lo), m_hi(hi), m_decimals(decimals), m_step(step) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(0, 10000);
    m_slider->setToolTip(QStringLiteral("Arraste para ajuste rapido"));
    layout->addWidget(m_slider, 1);

    m_spin = new QDoubleSpinBox(this);
    m_spin->setDecimals(decimals);
    m_spin->setRange(lo, hi);
    m_spin->setSingleStep(step);
    m_spin->setValue(value);
    m_spin->setMinimumWidth(70);
    m_spin->setKeyboardTracking(false);   // so emite ao perder o foco ou Enter
    layout->addWidget(m_spin);

    // O slider e limitado a uma faixa util mesmo quando o valor pode passar
    // dela: -180..180 num angulo, por exemplo.
    const double sliderLo = std::max(lo, -1000.0);
    const double sliderHi = std::min(hi, 1000.0);
    const auto rangeOf = [this](double v) {
        const double sliderLo = std::max(m_lo, -1000.0);
        const double sliderHi = std::min(m_hi, 1000.0);
        if (sliderHi <= sliderLo) return 0;
        return static_cast<int>(std::clamp(
            (v - sliderLo) / (sliderHi - sliderLo) * 10000.0, 0.0, 10000.0));
    };
    Q_UNUSED(sliderLo);
    Q_UNUSED(sliderHi);
    m_slider->setValue(rangeOf(value));

    connect(m_slider, &QSlider::valueChanged, this, &NumberEditor::syncFromSlider);
    connect(m_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &NumberEditor::syncFromSpin);
    connect(m_spin, &QDoubleSpinBox::editingFinished, this,
            &NumberEditor::editingFinished);
}

void NumberEditor::setValueQuiet(const Value& v) {
    m_updating = true;
    const double d = v.asDouble(0.0);
    m_spin->setValue(d);
    const double sliderLo = std::max(m_lo, -1000.0);
    const double sliderHi = std::min(m_hi, 1000.0);
    if (sliderHi > sliderLo) {
        m_slider->setValue(static_cast<int>(std::clamp(
            (d - sliderLo) / (sliderHi - sliderLo) * 10000.0, 0.0, 10000.0)));
    }
    m_updating = false;
}

Value NumberEditor::value() const {
    return Value(m_spin->value());
}

void NumberEditor::setRange(double lo, double hi) {
    m_lo = lo;
    m_hi = hi;
    m_spin->setRange(lo, hi);
}

void NumberEditor::syncFromSlider() {
    if (m_updating) return;
    const double sliderLo = std::max(m_lo, -1000.0);
    const double sliderHi = std::min(m_hi, 1000.0);
    if (sliderHi <= sliderLo) return;
    const double v = sliderLo + (m_slider->value() / 10000.0) * (sliderHi - sliderLo);
    m_updating = true;
    m_spin->setValue(v);
    m_updating = false;
    emit valueChanged(m_spin->value());
}

void NumberEditor::syncFromSpin(double v) {
    if (m_updating) return;
    const double sliderLo = std::max(m_lo, -1000.0);
    const double sliderHi = std::min(m_hi, 1000.0);
    if (sliderHi > sliderLo) {
        m_updating = true;
        m_slider->setValue(static_cast<int>(std::clamp(
            (v - sliderLo) / (sliderHi - sliderLo) * 10000.0, 0.0, 10000.0)));
        m_updating = false;
    }
    emit valueChanged(v);
}

// ---------------------------------------------------------------------------
// ColorEditor
// ---------------------------------------------------------------------------

ColorEditor::ColorEditor(QWidget* parent, const Color& value)
    : QWidget(parent), m_color(value) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_swatch = new QPushButton(this);
    m_swatch->setFixedSize(28, 20);
    m_swatch->setToolTip(QStringLiteral("Clique para escolher a cor"));
    m_swatch->setFlat(true);
    layout->addWidget(m_swatch);

    m_label = new QLabel(this);
    m_label->setFont(monoFont(8));
    layout->addWidget(m_label, 1);

    connect(m_swatch, &QPushButton::clicked, this, &ColorEditor::chooseColor);
    updateSwatch();
}

void ColorEditor::updateSwatch() {
    const QColor q = toQColor(m_color);
    m_swatch->setStyleSheet(
        QStringLiteral("QPushButton { background-color: %1; border: 1px solid %2; }")
            .arg(q.name(QColor::HexArgb))
            .arg(q.alpha() < 255 ? QStringLiteral("#888888") : QStringLiteral("#363b45")));
    m_label->setText(formatColor(m_color));
}

void ColorEditor::chooseColor() {
    const QColor initial = toQColor(m_color);
    const QColor chosen = QColorDialog::getColor(initial, this,
                                                  QStringLiteral("Escolher cor"), true);
    if (!chosen.isValid()) return;
    m_color = fromQColor(chosen);
    updateSwatch();
    emit valueChanged(m_color);
    emit editingFinished();
}

void ColorEditor::setValueQuiet(const Value& v) {
    m_updating = true;
    m_color = v.asColor();
    updateSwatch();
    m_updating = false;
}

Value ColorEditor::value() const {
    return Value(m_color);
}

// ---------------------------------------------------------------------------
// TextEditor
// ---------------------------------------------------------------------------

TextEditor::TextEditor(QWidget* parent, const std::string& value, bool isFilePath)
    : QLineEdit(parent), m_isFilePath(isFilePath) {
    setText(QString::fromStdString(value));
    setMinimumWidth(60);
    setFont(monoFont(8));

    if (isFilePath) {
        // O campo e o browse precisam ficar lado a lado dentro do mesmo
        // editor, senao o browse cai em outra linha e quebra o layout.
        m_holder = new QWidget(this);
        m_layout = new QHBoxLayout(m_holder);
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->setSpacing(2);
        m_layout->addWidget(this, 1);

        m_browse = new QPushButton(QStringLiteral("..."), m_holder);
        m_browse->setFixedWidth(26);
        m_browse->setToolTip(QStringLiteral("Escolher arquivo"));
        m_layout->addWidget(m_browse);

        connect(m_browse, &QPushButton::clicked, this, &TextEditor::chooseFile);
    }
    connect(this, &QLineEdit::editingFinished, this, [this] {
        emit valueChanged(text());
    });
}

QWidget* TextEditor::widget() {
    return m_holder ? m_holder : static_cast<QWidget*>(this);
}

void TextEditor::chooseFile() {
    const QString filter = m_isFilePath
                               ? QStringLiteral("Todos os arquivos (*);;Video (*.mp4 *.mov *.mkv *.avi);;Imagem (*.png *.jpg *.tif *.tiff *.exr);;Audio (*.wav *.mp3 *.flac)")
                               : QStringLiteral("Todos os arquivos (*)");
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Escolher arquivo"),
                                                      text(), filter);
    if (!path.isEmpty()) setText(path);
}

void TextEditor::setValueQuiet(const Value& v) {
    m_updating = true;
    setText(QString::fromStdString(v.asString()));
    m_updating = false;
}

Value TextEditor::value() const {
    return Value(text().toStdString());
}

void TextEditor::setHasKeyframe(bool on) {
    // Texto puro nao se anima; um caminho pode virar "source" com keyframe.
    setEnabled(on);
    setPlaceholderText(on ? QString() : QStringLiteral("fixo"));
}

// ---------------------------------------------------------------------------
// EnumEditor
// ---------------------------------------------------------------------------

EnumEditor::EnumEditor(QWidget* parent, const std::string& value,
                       const std::vector<std::string>& options)
    : QComboBox(parent) {
    for (const auto& option : options) {
        addItem(QString::fromStdString(option), QString::fromStdString(option));
    }
    const int index = findData(QString::fromStdString(value));
    setCurrentIndex(index >= 0 ? index : 0);

    connect(this, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        if (m_updating) return;
        emit valueChanged(text);
    });
}

void EnumEditor::setValueQuiet(const Value& v) {
    m_updating = true;
    const int index = findData(QString::fromStdString(v.asString()));
    if (index >= 0) setCurrentIndex(index);
    m_updating = false;
}

Value EnumEditor::value() const {
    return Value(currentData().toString().toStdString());
}

void EnumEditor::setHasKeyframe(bool on) {
    setEnabled(true);
    Q_UNUSED(on);
}

// ---------------------------------------------------------------------------
// VectorEditor
// ---------------------------------------------------------------------------

VectorEditor::VectorEditor(QWidget* parent, const Vec2& value, double lo, double hi,
                           int decimals)
    : QWidget(parent), m_count(2), m_lo(lo), m_hi(hi), m_decimals(decimals) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    const char* labels[3] = {"X", "Y", "Z"};
    const double values[3] = {value.x, value.y, 0.0};
    for (int i = 0; i < m_count; ++i) {
        auto* field = new QDoubleSpinBox(this);
        field->setDecimals(decimals);
        field->setRange(lo, hi);
        field->setSingleStep(1.0);
        field->setValue(values[i]);
        field->setPrefix(QString(1, QLatin1Char(' ')) + QLatin1Char(' ') +
                         QString::fromLatin1(labels[i]));
        field->setMinimumWidth(72);
        m_fields[i] = field;
        layout->addWidget(field, 1);

        connect(field, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this] {
                    if (m_updating) return;
                    emit valueChanged(m_fields[0]->value(), m_fields[1]->value(),
                                      m_count > 2 ? m_fields[2]->value() : 0.0);
                });
    }
}

VectorEditor::VectorEditor(QWidget* parent, const Vec3& value, double lo, double hi,
                           int decimals)
    : QWidget(parent), m_count(3), m_lo(lo), m_hi(hi), m_decimals(decimals) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    const char* labels[3] = {"X", "Y", "Z"};
    const double values[3] = {value.x, value.y, value.z};
    for (int i = 0; i < 3; ++i) {
        auto* field = new QDoubleSpinBox(this);
        field->setDecimals(decimals);
        field->setRange(lo, hi);
        field->setSingleStep(1.0);
        field->setValue(values[i]);
        field->setPrefix(QString::fromLatin1(labels[i]));
        field->setMinimumWidth(64);
        m_fields[i] = field;
        layout->addWidget(field, 1);

        connect(field, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this] {
                    if (m_updating) return;
                    emit valueChanged(m_fields[0]->value(), m_fields[1]->value(),
                                      m_fields[2]->value());
                });
    }
}

void VectorEditor::setValueQuiet(const Value& v) {
    m_updating = true;
    if (m_count == 2) {
        const Vec2 p = v.asVec2();
        m_fields[0]->setValue(p.x);
        m_fields[1]->setValue(m_invertY ? -p.y : p.y);
    } else {
        const Vec3 p = v.asVec3();
        m_fields[0]->setValue(p.x);
        m_fields[1]->setValue(p.y);
        m_fields[2]->setValue(p.z);
    }
    m_updating = false;
}

Value VectorEditor::value() const {
    if (m_count == 2) {
        return Value(Vec2{m_fields[0]->value(),
                          m_invertY ? -m_fields[1]->value() : m_fields[1]->value()});
    }
    return Value(Vec3{m_fields[0]->value(), m_fields[1]->value(),
                      m_fields[2]->value()});
}

void VectorEditor::setHasKeyframe(bool on) {
    for (int i = 0; i < m_count; ++i) m_fields[i]->setEnabled(on);
}

// ---------------------------------------------------------------------------
// BoolEditor
// ---------------------------------------------------------------------------

BoolEditor::BoolEditor(QWidget* parent, bool value) : QCheckBox(parent) {
    setChecked(value);
    setTristate(false);
    connect(this, &QCheckBox::toggled, this, [this](bool v) {
        if (m_updating) return;
        emit valueChanged(v);
    });
}

void BoolEditor::setValueQuiet(const Value& v) {
    m_updating = true;
    setChecked(v.asBool(false));
    m_updating = false;
}

Value BoolEditor::value() const {
    return Value(isChecked());
}

void BoolEditor::setHasKeyframe(bool on) {
    setEnabled(true);
    Q_UNUSED(on);
}

// ---------------------------------------------------------------------------
// AngleEditor
// ---------------------------------------------------------------------------

void AngleEditor::setValueQuiet(const Value& v) {
    // O modelo guarda graus; nada a converter, mas a funcao existe para que o
    // inspector trate todos os tipos da mesma forma.
    NumberEditor::setValueQuiet(v);
}

}  // namespace lmn::ui
