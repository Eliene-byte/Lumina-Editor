// Theme.h - Cores e medidas compartilhadas pelos paineis.
//
// Tudo em um lugar so: os paineis desenham com QPainter (rapido e sem um
// widget por item) e precisam concordar nas cores. Um unico arquivo evita a
// situacao classica de a timeline e o visualizador ficarem com paletas
// diferentes.
#pragma once

#include <QColor>
#include <QFont>
#include <QString>

namespace lmn::ui {

struct Palette {
    QColor background;        // fundo do aplicativo
    QColor surface;           // fundo de painel
    QColor surfaceAlt;        // linhas alternadas, trilhos
    QColor surfaceHover;
    QColor border;
    QColor borderStrong;
    QColor text;
    QColor textDim;
    QColor textInverted;
    QColor accent;            // selecao, foco, reproducao
    QColor accentDim;
    QColor warning;
    QColor danger;
    QColor success;
    QColor keyframe;          // diamante de keyframe
    QColor keyframeSelected;
    QColor playhead;
    QColor graphBackground;
    QColor graphGrid;
    QColor graphWire;
    QColor graphWireActive;
    QColor nodeHeader;        // cor padrao do cabecalho de no
    QColor videoWaveform;

    [[nodiscard]] static Palette dark();
};

struct Metrics {
    int rowHeight = 22;        // linha da timeline
    int trackHeaderWidth = 150;
    int rulerHeight = 26;
    int nodeHeaderHeight = 22;
    int nodeMinWidth = 150;
    int nodeHeaderWidth = 130;
    int nodeCornerRadius = 5;
    int portRadius = 4;
    int gridStep = 20;
    int timelineRulerStep = 50;  // pixels por segundo, ajustado por zoom
    double timeRulerStep = 1.0;  // segundos entre marcas

    [[nodiscard]] static Metrics scaled(double uiScale);
};

// Fonte monoespacada para numeros: alinhar digitos verticalmente importa em
// campos de tempo e valores.
[[nodiscard]] QFont monoFont(int pointSize = 0);
[[nodiscard]] QFont uiFont(int pointSize = 0, bool bold = false);

}  // namespace lmn::ui
