#include "Theme.h"

#include <QFontDatabase>
#include <QStringList>

namespace lmn::ui {

Palette Palette::dark() {
    Palette p;
    p.background     = QColor(0x1c, 0x1e, 0x22);
    p.surface        = QColor(0x21, 0x24, 0x29);
    p.surfaceAlt     = QColor(0x1a, 0x1c, 0x20);
    p.surfaceHover   = QColor(0x26, 0x2a, 0x31);
    p.border         = QColor(0x2f, 0x33, 0x3b);
    p.borderStrong   = QColor(0x44, 0x4b, 0x57);
    p.text           = QColor(0xd6, 0xd9, 0xde);
    p.textDim        = QColor(0x8b, 0x91, 0x9c);
    p.textInverted   = QColor(0xff, 0xff, 0xff);
    p.accent         = QColor(0x3d, 0x6f, 0xd4);
    p.accentDim      = QColor(0x2f, 0x4a, 0x80);
    p.warning        = QColor(0xd4, 0x9b, 0x3d);
    p.danger         = QColor(0xd4, 0x53, 0x53);
    p.success        = QColor(0x5c, 0xa0, 0x6a);
    p.keyframe       = QColor(0xe0, 0xb0, 0x50);
    p.keyframeSelected = QColor(0xff, 0xd4, 0x6b);
    p.playhead       = QColor(0xe8, 0x54, 0x54);
    p.graphBackground = QColor(0x17, 0x19, 0x1c);
    p.graphGrid      = QColor(0x22, 0x25, 0x2a);
    p.graphWire      = QColor(0x5a, 0x62, 0x6e);
    p.graphWireActive = QColor(0x7f, 0xa8, 0xf0);
    p.nodeHeader     = QColor(0x39, 0x41, 0x4e);
    p.videoWaveform  = QColor(0x6c, 0x8a, 0xb8);
    return p;
}

Metrics Metrics::scaled(double uiScale) {
    Metrics m;
    const auto px = [uiScale](int v) { return std::max(1, static_cast<int>(v * uiScale)); };
    m.rowHeight = px(22);
    m.trackHeaderWidth = px(150);
    m.rulerHeight = px(26);
    m.nodeHeaderHeight = px(22);
    m.nodeMinWidth = px(150);
    m.nodeHeaderWidth = px(130);
    m.nodeCornerRadius = px(5);
    m.portRadius = px(4);
    m.gridStep = px(20);
    return m;
}

QFont monoFont(int pointSize) {
    static const QStringList families = {
        QStringLiteral("Cascadia Mono"), QStringLiteral("Consolas"),
        QStringLiteral("JetBrains Mono"), QStringLiteral("DejaVu Sans Mono"),
        QStringLiteral("Courier New"),
    };
    for (const QString& family : families) {
        if (QFontDatabase::families().contains(family, QFontDatabase::FixedPitch)) {
            QFont f(family);
            if (pointSize > 0) f.setPointSize(pointSize);
            return f;
        }
    }
    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (pointSize > 0) f.setPointSize(pointSize);
    return f;
}

QFont uiFont(int pointSize, bool bold) {
    QFont f = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    f.setPointSize(pointSize > 0 ? pointSize : f.pointSize());
    f.setBold(bold);
    return f;
}

}  // namespace lmn::ui
