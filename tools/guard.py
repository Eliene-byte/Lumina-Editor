#!/usr/bin/env python3
"""
guard.py - Verificador estatico e corrigidor automatico do Lumina.

Existe por um motivo concreto: sem compilador local, cada erro de codigo
custava um ciclo de CI de cinco minutos. Este script encontra a mesma classe
de erro em segundos, e corrige sozinho o que e mecanico.

USO
    python tools/guard.py              # reporta os problemas
    python tools/guard.py --fix        # reporta e corrige o que der
    python tools/guard.py --quiet      # so o resumo (para o CI)
    python tools/guard.py --rule=X     # roda so a regra X

SAIDA
    Codigo 0: tudo certo
    Codigo 1: ha problemas nao corrigidos (o CI falha)
    Codigo 2: problema interno do proprio guard

O QUE ELE SABE VERIFICAR
    include-missing     Usa uma classe Qt/std sem incluir o cabecalho
    qt-module           A classe e de um modulo que o target nao linka
    decl-without-def    Metodo declarado no header, sem definicao no .cpp (ERRO DE LINK)
    def-without-decl    Definicao no .cpp sem declaracao no header
    member-missing      m_x usado no .cpp sem estar declarado na classe
    signal-slot         connect() com aridade de signal e slot diferentes
    cmake-target        target_link_libraries aponta para target inexistente
    cmake-source        .cpp existe no disco e nao esta no CMakeLists
    qt-signal           Sinal inexistente do Qt (lista curada)

O QUE ELE CORRIGE (--fix)
    include-missing     Acrescenta o #include
    qt-module           Acrescenta o componente em find_package
    cmake-source        Acrescenta o arquivo no CMakeLists

O QUE ELE NAO CORRIGE
    Erros de semantica, de logica e de design. Isso exige alguem que pense.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path

# ---------------------------------------------------------------------------
# Conheco do Qt: classe -> modulo, cabecalho
#
# Derivado de verdade do Qt instalado, nao de memoria. Um mapa errado aqui
# produz um include errado, que e pior do que nenhum include.
# ---------------------------------------------------------------------------

QT_CLASS_TO_MODULE: dict[str, str] = {}
QT_CLASS_TO_HEADER: dict[str, str] = {}

# Fallback para quando o Qt nao esta instalado. Cobrindo o que o projeto usa.
_FALLBACK = {
    "QObject": "Core", "QString": "Core", "QVariant": "Core", "QSaveFile": "Core",
    "QJsonDocument": "Core", "QJsonObject": "Core", "QJsonArray": "Core",
    "QJsonValue": "Core", "QJsonParseError": "Core", "QByteArray": "Core",
    "QRect": "Core", "QRectF": "Core", "QPoint": "Core", "QPointF": "Core",
    "QSize": "Core", "QSizeF": "Core", "QMargins": "Core", "QDateTime": "Core",
    "QDate": "Core", "QTime": "Core", "QTimer": "Core", "QElapsedTimer": "Core",
    "QMutex": "Core", "QMutexLocker": "Core", "QReadWriteLock": "Core",
    "QThread": "Core", "QMetaType": "Core", "QCoreApplication": "Core",
    "QCommandLineParser": "Core", "QCommandLineOption": "Core", "QSettings": "Core",
    "QEvent": "Core", "QFileInfo": "Core", "QUrl": "Core", "QLocale": "Core",
    "QRandomGenerator": "Core", "QTextStream": "Core", "QDataStream": "Core",
    "QRegularExpression": "Core", "QCryptographicHash": "Core", "QIODevice": "Core",
    "QMetaObject": "Core", "QLibraryInfo": "Core", "QOperatingSystemVersion": "Core",
    "QSignalBlocker": "Core", "QPersistentModelIndex": "Core", "QAbstractItemModel": "Core",
    "QModelIndex": "Core", "QAbstractListModel": "Core", "QAbstractItemView": "Core",
    "QAbstractScrollArea": "Core", "QKeyEvent": "Core", "QMouseEvent": "Core",
    "QWheelEvent": "Core", "QResizeEvent": "Core", "QCloseEvent": "Core",
    "QContextMenuEvent": "Core", "QDragEnterEvent": "Core", "QDropEvent": "Core",
    "QFocusEvent": "Core", "QPaintEvent": "Core", "QHelpEvent": "Core",
    "QAction": "Gui", "QUndoStack": "Gui", "QUndoCommand": "Gui",
    "QUndoGroup": "Gui", "QKeySequence": "Gui", "QClipboard": "Gui",
    "QFont": "Gui", "QFontDatabase": "Gui", "QIcon": "Gui", "QPixmap": "Gui",
    "QImage": "Gui", "QImageReader": "Gui", "QColor": "Gui", "QPalette": "Gui",
    "QCursor": "Gui", "QFontMetrics": "Gui", "QPainterPath": "Gui",
    "QPolygonF": "Gui", "QSurfaceFormat": "Gui", "QOffscreenSurface": "Gui",
    "QOpenGLContext": "Gui", "QOpenGLShader": "Gui", "QGuiApplication": "Gui",
    "QScreen": "Gui", "QCursorShape": "Gui", "QWindow": "Gui",
    "QOpenGLFunctions_3_3_Core": "OpenGL", "QOpenGLFunctions": "OpenGL",
    "QOpenGLShaderProgram": "OpenGL", "QOpenGLBuffer": "OpenGL",
    "QOpenGLFramebufferObject": "OpenGL", "QOpenGLTexture": "OpenGL",
    "QOpenGLContext": "Gui", "QOpenGLShader": "Gui",
    "QOpenGLWidget": "OpenGLWidgets", "QOpenGLWindow": "OpenGLWidgets",
    "QWidget": "Widgets", "QLabel": "Widgets", "QPushButton": "Widgets",
    "QToolButton": "Widgets", "QCheckBox": "Widgets", "QRadioButton": "Widgets",
    "QComboBox": "Widgets", "QSlider": "Widgets", "QDial": "Widgets",
    "QSpinBox": "Widgets", "QDoubleSpinBox": "Widgets", "QLineEdit": "Widgets",
    "QTextEdit": "Widgets", "QPlainTextEdit": "Widgets", "QGroupBox": "Widgets",
    "QLayout": "Widgets", "QVBoxLayout": "Widgets", "QHBoxLayout": "Widgets",
    "QGridLayout": "Widgets", "QFormLayout": "Widgets", "QStackedLayout": "Widgets",
    "QWidgetItem": "Widgets", "QSpacerItem": "Widgets", "QFrame": "Widgets",
    "QScrollArea": "Widgets", "QTabWidget": "Widgets", "QTabBar": "Widgets",
    "QSplitter": "Widgets", "QDockWidget": "Widgets", "QMainWindow": "Widgets",
    "QMenuBar": "Widgets", "QMenu": "Widgets", "QStatusBar": "Widgets",
    "QToolBar": "Widgets", "QMessageBox": "Widgets", "QFileDialog": "Widgets",
    "QColorDialog": "Widgets", "QDialog": "Widgets", "QDialogButtonBox": "Widgets",
    "QProgressBar": "Widgets", "QTreeWidget": "Widgets", "QTreeWidgetItem": "Widgets",
    "QListWidget": "Widgets", "QHeaderView": "Widgets", "QApplication": "Widgets",
    "QTableWidget": "Widgets", "QStyledItemDelegate": "Widgets",
    "QStyleOptionViewItem": "Widgets", "QStyle": "Widgets", "QSizePolicy": "Widgets",
    "QFontMetricsF": "Gui", "QTextDocument": "Gui",
}

# include que satisfaz cada classe
_CLASS_INCLUDE: dict[str, str] = {}

# Modulos que este projeto nao usa. Sem esse filtro o mapa ganha ~2400
# classes de Sql/Concurrent/Test, e a regra de modulo passa a exigir Qt6::Sql
# por causa de um QSqlError citado num comentario.
_SKIP_MODULES = {
    "Sql", "Test", "TestWidgets", "Concurrent", "DBus", "Network", "PrintSupport",
    "Designer", "Help", "Multimedia", "MultimediaWidgets", "Charts", "DataVisualization",
    "OpenGLWidgets3D", "Quick3D", "Quick", "Qml", "QuickWidgets", "RemoteObjects",
    "Scxml", "Sensors", "SerialPort", "Positioning", "Bluetooth", "Nfc", "WebChannel",
    "WebSockets", "WebView", "SpatialAudio", "TextToSpeech", "UiTools",
}



def load_qt_knowledge(qt_root: Path | None) -> None:
    """
    Le a arvore de includes do Qt instalado e monta o mapa classe -> modulo.
    Sem isso, o guard so conhece o que esta em _FALLBACK, e erra em classe
    nova. Com ele, o mapa e sempre verdade.
    """
    QT_CLASS_TO_MODULE.clear()
    QT_CLASS_TO_MODULE.update(_FALLBACK)
    QT_CLASS_TO_HEADER.clear()

    # Monta classe -> header a partir do fallback; refinado abaixo pelo disco.
    for cls, module in _FALLBACK.items():
        QT_CLASS_TO_HEADER[cls] = cls


    if qt_root is None or not qt_root.is_dir():
        return

    include_dir = qt_root / "include"
    if not include_dir.is_dir():
        return

    # O nome do arquivo NAO e o nome da classe. O header se chama
    # qopenglwidget.h e a classe se chama QOpenGLWidget, com o "OpenGL" em
    # maiusculas que ninguem consegue deduzir do arquivo. A versao que usava
    # o nome do arquivo guardava 2400 chaves minusculas inuteis e nao achava
    # a classe nenhuma, entao a regra de modulo Qt rodava sem nunca acusar.
    # Agora o nome vem da declaracao real dentro do header.
    class_re = re.compile(
        r"\b(?:class|struct)\s+(?:[A-Za-z_]\w*_EXPORT\s+)?(Q[A-Za-z0-9_]{2,})"
    )
    for module_dir in sorted(include_dir.iterdir()):
        if not module_dir.is_dir() or not module_dir.name.startswith("Qt"):
            continue
        module = module_dir.name[2:]  # tira o "Qt"
        if module in _SKIP_MODULES:
            continue
        for header in sorted(module_dir.iterdir()):
            if not header.is_file() or header.suffix not in (".h", ".hpp"):
                continue
            text = read(header)
            if "class" not in text and "struct" not in text:
                continue
            found = set(class_re.findall(text))
            if not found:
                continue
            for cls in found:
                QT_CLASS_TO_MODULE.setdefault(cls, module)
                QT_CLASS_TO_HEADER.setdefault(cls, header.stem)



# ---------------------------------------------------------------------------
# Achados
# ---------------------------------------------------------------------------


@dataclass
class Finding:
    rule: str
    file: str
    line: int
    message: str
    fixable: bool = False
    fix: str | None = None          # patch pronto, em diff unificado
    fix_hint: str | None = None    # o que o guard sabe fazer

    def key(self) -> tuple:
        return (self.rule, self.file, self.line, self.message)


SEV = {
    "decl-without-def": "ERRO",
    "qt-module": "ERRO",
    "cmake-target": "ERRO",
    "qt-signal": "ERRO",
    "member-missing": "ERRO",
    "include-missing": "ERRO",
    "def-without-decl": "ERRO",
    "signal-slot": "AVISO",
    "cmake-source": "AVISO",
}


# ---------------------------------------------------------------------------
# Utilidades de leitura
# ---------------------------------------------------------------------------


def read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def strip_comments_and_strings(text: str) -> str:
    """
    Remove comentarios e strings, preservando o numero de linha.
    Sem isso, um "QWidget" dentro de um comentario dispara um falso positivo,
    e um guard que reclama de coisa errada deixa de ser lido.
    """
    out = []
    i = 0
    n = len(text)
    state = None  # None, 'line', 'block', '"', "'"
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if state is None:
            if c == "/" and nxt == "/":
                state = "line"
                out.append("  ")
                i += 2
                continue
            if c == "/" and nxt == "*":
                state = "block"
                out.append("  ")
                i += 2
                continue
            if c == '"':
                state = '"'
                out.append(" ")
                i += 1
                continue
            if c == "'":
                state = "'"
                out.append(" ")
                i += 1
                continue
            out.append(c)
            i += 1
            continue
        if state == "line":
            if c == "\n":
                state = None
                out.append("\n")
            else:
                out.append(" ")
            i += 1
            continue
        if state == "block":
            if c == "*" and nxt == "/":
                state = None
                out.append("  ")
                i += 2
                continue
            out.append("\n" if c == "\n" else " ")
            i += 1
            continue
        # string
        if c == "\\" and nxt:
            out.append("  " if nxt == "\n" else " ")
            i += 2
            continue
        if (state == '"' and c == '"') or (state == "'" and c == "'"):
            state = None
            out.append(" ")
            i += 1
            continue
        out.append("\n" if c == "\n" else " ")
        i += 1
    return "".join(out)


def line_of(text: str, index: int) -> int:
    return text.count("\n", 0, index) + 1


# target_link_libraries(alvo ...) ... fecha num ')' que comeca a linha.
#
# Dois detalhes que custaram dois bugs reais:
#
#  1. O nome precisa de lookahead. Sem ele "lumina_media" casava como prefixo
#     de "lumina_media_ui" e as dependencias do alvo errado iam para o alvo
#     errado.
#  2. O fecha precisa ser \n[ \t]*\) e nao \n\). Com o padrao burro, um
#     bloco dentro de if() - que fecha com "    )" indentado - engolia o
#     bloco seguinte inteiro. Foi assim que lumina_media apareceu sem
#     Qt6::OpenGL, embora o arquivo linkasse.
_LINK_BLOCK = re.compile(
    r"target_link_libraries\(\s*([A-Za-z_]\w*)(?![A-Za-z0-9_])([\s\S]*?)\n[ \t]*\)"
)


def link_blocks(text: str) -> list[tuple[str, str, int]]:
    """(alvo, corpo, offset) de cada target_link_libraries do arquivo."""
    return [(m.group(1), m.group(2), m.start()) for m in _LINK_BLOCK.finditer(text)]



def find_include_insert_point(lines: list[str]) -> int:
    """Depois do ultimo #include, mantendo o agrupamento de includes."""
    last = -1
    for idx, line in enumerate(lines):
        if line.startswith("#include"):
            last = idx
    if last >= 0:
        return last + 1
    for idx, line in enumerate(lines):
        if line.startswith("#pragma") or line.startswith("namespace"):
            return idx
    return 0


# ---------------------------------------------------------------------------
# Regras
# ---------------------------------------------------------------------------


class Guard:
    def __init__(self, root: Path, qt_root: Path | None, rules: set[str] | None, want_includes: bool = False):
        self.root = root
        self.qt_root = qt_root
        self.rules = rules
        self.want_includes = want_includes
        self.findings: list[Finding] = []
        self.sources: list[Path] = sorted(
            p for p in root.rglob("*")
            if p.suffix in (".cpp", ".h") and ".git" not in p.parts and "build" not in p.parts
        )
        self.cmake_files = sorted(root.rglob("CMakeLists.txt"))
        self.cmake_files = [p for p in self.cmake_files if ".git" not in p.parts]
        self._cmake_targets: dict[str, str] = {}
        self._cmake_aliases: set[str] = set()
        self._target_libs: dict[str, set[str]] = defaultdict(set)
        self._find_modules: dict[Path, set[str]] = {}
        self._class_index: dict[str, tuple[Path, str]] = {}
        self._symbol_cache: dict[str, str | None] = {}
        self._headers = [p for p in self.sources if p.suffix == ".h"]
        self._index()

    def enabled(self, rule: str) -> bool:
        return self.rules is None or rule in self.rules

    def add(self, finding: Finding) -> None:
        if not self.enabled(finding.rule):
            return
        if any(f.key() == finding.key() for f in self.findings):
            return
        self.findings.append(finding)

    # -- indexacao ------------------------------------------------------------

    def _index(self) -> None:
        for path in self.cmake_files:
            text = read(path)
            for m in re.finditer(
                r"add_(?:library|executable)\(\s*([A-Za-z_]\w*(?:::[A-Za-z_]\w*)?)",
                text,
            ):
                self._cmake_targets[m.group(1)] = str(path)

            # add_library(lumina::ui ALIAS lumina_ui). O nome espaco de nomes
            # tem "::", que o padrao de nome de alvo acima nao aceitava - por
            # isso nenhum alias era reconhecido e todo lumina::core era
            #_reportado_ como inexistente.
            for m in re.finditer(
                r"add_(?:library|executable)\(\s*"
                r"([A-Za-z_]\w*(?:::[A-Za-z_]\w*)?)\s+ALIAS\s+([A-Za-z_]\w*)",
                text,
            ):
                self._cmake_aliases.add(m.group(1))
                self._cmake_aliases.add(m.group(2))


            for m in re.finditer(
                r"target_link_libraries\(\s*([A-Za-z_]\w*)([\s\S]*?)\n\)", text
            ):
                target, body = m.group(1), m.group(2)
                for d in re.finditer(r"([A-Za-z_]\w*)::([A-Za-z_]\w*)", body):
                    self._target_libs[target].add(d.group(1))
                for d in re.finditer(r"(?<![:\w])(lumina_\w+)", body):
                    self._target_libs[target].add(d.group(1))

            for m in re.finditer(
                r"find_package\(Qt6[^)]*?COMPONENTS\s+([^)]*?)\)", text, re.S
            ):
                self._find_modules[path] = {
                    c for c in re.findall(r"[A-Za-z_]\w*", m.group(1))
                }

        # Classe -> (arquivo, classe) para checar membros
        for path in self.sources:
            if path.suffix != ".h":
                continue
            text = strip_comments_and_strings(read(path))
            for m in re.finditer(r"\bclass\s+(\w+)\b", text):
                self._class_index.setdefault(m.group(1), (path, m.group(1)))

    # -- regra: include ausente ----------------------------------------------

    # Headers de C++ padrao e da biblioteca C: includer e obrigatorio, e o
    # compilador avisa com arquivo:linha. Vale a pena checar.
    STD_HEADERS = {
        "algorithm", "any", "array", "atomic", "barrier", "bit", "bitset", "cassert",
        "cctype", "charconv", "chrono", "cmath", "compare", "complex", "concepts",
        "condition_variable", "cstddef", "cstdint", "cstdio", "cstdlib", "cstring",
        "ctime", "deque", "exception", "execution", "filesystem", "format",
        "forward_list", "fstream", "functional", "future", "initializer_list",
        "iomanip", "ios", "iosfwd", "iostream", "istream", "iterator", "limits",
        "list", "locale", "map", "memory", "mutex", "new", "numeric", "optional",
        "ostream", "queue", "random", "ranges", "ratio", "regex", "scoped_allocator",
        "set", "shared_mutex", "source_location", "span", "sstream", "stack",
        "stdexcept", "stop_token", "streambuf", "string", "string_view", "system_error",
        "thread", "tuple", "type_traits", "typeindex", "typeinfo", "unordered_map",
        "unordered_set", "utility", "valarray", "variant", "vector", "version",
    }

    def check_includes(self) -> None:
        """
        Headers da biblioteca padrao que o arquivo usa e nao inclui.

        Esta regra e OTATIVA por padrao (--includes), e o motivo e medido: com
        ela ligada o guard reportou 524 achados, dos quais quase todos eram
        <string>, <vector> e <format> chegando por include transitivo. Um
        verificador que grita lobo 524 vezes deixa de ser lido, e as quatro
        regras que tem precisao real se perdem no ruido.

        Para quem compila com MSVC, a include transitiva funciona. O que broke
        de verdade no Lumina foi modulo de Qt errado (regra qt-module), metodo
        declarado sem definicao (decl-without-def) e membro inexistente
        (member-missing). Essas ficam sempre ligadas.
        """
        for path in self.sources:
            raw = read(path)
            if not raw:
                continue
            code = strip_comments_and_strings(raw)
            code_lines = code.splitlines()

            incluidos: set[str] = set()
            for m in re.finditer(r'#include\s+<([^>]+)>', code):
                incluidos.add(m.group(1))
            for m in re.finditer(r'#include\s+"([^"]+)"', code):
                incluidos.add(m.group(1))

            usados: dict[str, int] = {}
            for idx, line in enumerate(code_lines):
                if "#include" in line:
                    continue
                for ident in re.findall(r"\b[A-Za-z_]\w*\b", line):
                    usados.setdefault(ident, idx + 1)

            for ident, lineno in sorted(usados.items(), key=lambda kv: kv[1]):
                if ident in self.STD_HEADERS and ident not in incluidos:
                    self.add(Finding(
                        rule="include-missing",
                        file=self._rel(path),
                        line=lineno,
                        message=f"usa <{ident}> sem #include <{ident}>",
                        fixable=True,
                        fix_hint=f"acrescentar #include <{ident}>",
                    ))

    def _first_use(self, code_lines: list[str], cls: str) -> int | None:
        for idx, line in enumerate(code_lines):
            if re.search(rf"\b{re.escape(cls)}\b", line):
                if "#include" in line:
                    continue
                return idx + 1
        return None

    def _comes_from_local_include(
        self, cls: str, included: set[str], path: Path
    ) -> bool:
        for inc in included:
            if not inc.startswith(("core/", "gpu/", "ui/", "expr/", "io/",
                                    "media/", "nodes/")):
                continue
            candidate = self.root / "src" / inc
            if candidate.is_file() and re.search(
                rf"\b{re.escape(cls)}\b", strip_comments_and_strings(read(candidate))
            ):
                return True
        return False

    # -- regra: modulo Qt que o target nao linka ---------------------------

    def check_qt_modules(self) -> None:
        """
        Classe de um modulo que o target nao linka. No Qt 6 isso nao da erro
        de configuracao: da erro de compilacao, tres passos depois, com uma
        mensagem sobre header nao encontrado que nao cita o modulo.
        """
        for path in self.cmake_files:
            if path.name != "CMakeLists.txt":
                continue
            text = read(path)
            for target, body, _off in link_blocks(text):
                linked: set[str] = set()
                for d in re.finditer(r"Qt6::([A-Za-z_]\w*)", body):
                    linked.add(d.group(1))
                for d in re.finditer(r"(?<![:\w])(lumina_\w+)", body):
                    linked.add(d.group(1))
                if not linked:
                    continue


                needed: set[str] = set()
                for src in self._sources_of(path, target):
                    code = strip_comments_and_strings(read(src))
                    usados = set(re.findall(r"\b[A-Za-z_]\w*\b", code))
                    for cls in usados & set(QT_CLASS_TO_MODULE):
                        module = QT_CLASS_TO_MODULE[cls]
                        if module in ("Core5Compat", "Test", "Sql", "Network"):
                            continue
                        if module in linked:
                            continue
                        needed.add(module)
                for module in sorted(needed):
                    self._require_qt_module(path, target, module, linked)

    def _sources_of(self, cmake_path: Path, target: str) -> list[Path]:
        text = read(cmake_path)
        m = re.search(
            rf"add_(?:library|executable)\(\s*{re.escape(target)}([\s\S]*?)\n\)", text
        )
        if not m:
            return []
        out = []
        for f in re.findall(r"([\w./\\-]+\.(?:cpp|c|h))", m.group(1)):
            candidate = (cmake_path.parent / f.replace("\\", "/")).resolve()
            if candidate.is_file():
                out.append(candidate)
            else:
                candidate2 = (self.root / f.replace("\\", "/")).resolve()
                if candidate2.is_file():
                    out.append(candidate2)
        return out

    def _require_qt_module(
        self, cmake_path: Path, target: str, module: str, linked: set[str]
    ) -> None:
        """
        Ha dois jeitos de faltar um modulo do Qt, e a versao anterior tratava
        os dois como um so: ela voltava cedo assim que o modulo aparecia em
        COMPONENTS, e por isso nunca accusava o caso mais comum - o modulo
        esta disponivel, mas o target nao linka. No Qt 6 isso da erro de
        link (simbolo indefinido), nao de configuracao, e nao aponta o modulo.
        """
        finder = next(
            (p for p in self.cmake_files if re.search(
                r"find_package\(Qt6[^)]*COMPONENTS", read(p))
            ),
            None,
        )
        ftext = read(finder) if finder else ""
        fm = re.search(r"find_package\(Qt6[^)]*?COMPONENTS\s+(.*?)\)", ftext, re.S) \
            if ftext else None
        have = set(re.findall(r"[A-Za-z_]\w*", fm.group(1))) if fm else set()

        if module not in have:
            # Nem no find_package: e preciso declarar E linkar.
            lineno = line_of(ftext, fm.start()) if fm else 1
            self.add(Finding(
                rule="qt-module",
                file=self._rel(finder) if finder else self._rel(cmake_path),
                line=lineno,
                message=(
                    f"target '{target}' usa classes de Qt{module}, mas nem "
                    f"find_package nem target_link_libraries citam Qt{module}"
                ),
                fixable=True,
                fix=self._patch_qt_module(finder, fm, module) if (finder and fm) else None,
                fix_hint=(
                    f"acrescentar Qt{module} em find_package e linkar Qt6::{module}"
                ),
            ))
            return

        # Esta no COMPONENTS, entao o alvo existe. Falta so linkar no alvo.
        if module in linked:
            return

        # O link fica no CMakeLists do DIRETORIO do alvo, nao no arquivo que
        # fez o find_package. Ler o arquivo errado aqui fez o guard acusar
        # lumina_media de nao linkar OpenGL, que ele linka.
        ctext = read(cmake_path)
        lineno = 1
        for name, _body, off in link_blocks(ctext):
            if name == target:
                lineno = line_of(ctext, off)
                break
        self.add(Finding(
            rule="qt-module",
            file=self._rel(cmake_path),
            line=lineno,
            message=(
                f"target '{target}' usa classes de Qt{module} mas nao linka "
                f"Qt6::{module} (o modulo esta no COMPONENTS, o link e que falta)"
            ),
            fixable=True,
            fix_hint=f"acrescentar Qt6::{module} em target_link_libraries({target})",
        ))


    def _patch_qt_module(
        self, finder: Path, fm: re.Match, module: str
    ) -> str:
        lines = read(finder).splitlines()
        # find_package: acrescenta uma linha com o componente
        end_line = fm.group(0).count("\n")
        insert_at = line_of(read(finder), fm.start()) - 1 + end_line
        lines.insert(insert_at, f"    {module}")
        return (
            f"--- a/{self._rel(finder)}\n"
            f"+++ b/{self._rel(finder)}\n"
            f"@@ -{insert_at + 1},0 +{insert_at + 1},1 @@\n"
            f"+    {module}\n"
        ) + _unified_tail(
            "".join(f"{ln}\n" for ln in lines[:insert_at]),
            "".join(f"{ln}\n" for ln in lines[insert_at:]),
        )

    # -- regra: declarado sem definido (erro de link) ------------------------

    def check_decl_def(self) -> None:
        for path in self.sources:
            if path.suffix != ".cpp":
                continue
            header = path.with_suffix(".h")
            if not header.is_file():
                continue

            cls = self._class_of(path, header)
            if cls is None:
                continue

            htext = strip_comments_and_strings(read(header))
            ctext = strip_comments_and_strings(read(path))
            body = self._class_body(htext, cls)

            declared = self._method_declarations(body)
            defined = {
                m.group(1)
                for m in re.finditer(rf"\b{re.escape(cls)}::(\w+)\s*\(", ctext)
            }
            # Classe aninhada: os metodos dela aparecem como Outer::Inner::metodo.
            # Sem isso, todo metodo de uma classe interna vira falso positivo.
            for inner in self._nested_class_names(body):
                defined |= {
                    m.group(1)
                    for m in re.finditer(
                        rf"\b{re.escape(cls)}::{re.escape(inner)}::(\w+)\s*\(", ctext
                    )
                }

            for name, lineno, is_slot in declared:
                if name in defined:
                    continue
                if is_slot:
                    continue
                if name in (cls, f"~{cls}"):
                    continue
                # Definido inline no header
                if re.search(
                    rf"\b{re.escape(name)}\s*\([^;{{]*\)\s*(?:const\s*)?"
                    r"(?:noexcept\s*)?(?:override\s*)?\{{",
                    body,
                ):
                    continue
                # Definido = delete / = default dentro da classe
                if re.search(
                    rf"\b{re.escape(name)}\s*\([^;]*\)\s*(?:const\s*)?=\s*"
                    r"(?:default|delete|0)\s*;",
                    body,
                ):
                    continue

                self.add(Finding(
                    rule="decl-without-def",
                    file=self._rel(header),
                    line=lineno,
                    message=(
                        f"{cls}::{name} declarado mas sem definicao em "
                        f"{path.name} (erro de link)"
                    ),
                ))

    def _method_declarations(self, body: str) -> list[tuple[str, int, bool]]:
        """
        Assinaturas dentro do corpo da classe, com a linha original.

        Ignora o que estiver em 'public slots:' ou 'signals:': nesses casos o
        moc gera a definicao, e reclamar seria falso positivo em todo
        widget do projeto.
        """
        out: list[tuple[str, int, bool]] = []
        current_is_slot = False
        for idx, line in enumerate(body.splitlines()):
            stripped = line.strip()
            if re.match(r"(?:public|private|protected)\s+slots\s*:", stripped):
                current_is_slot = True
                continue
            if re.match(r"^(?:public|private|protected)?\s*signals\s*:", stripped):
                current_is_slot = True
                continue
            if stripped.endswith(":") and not stripped.startswith("//"):
                # Qualquer outra secao volta a ser metodo comum
                current_is_slot = False
                continue
            if current_is_slot:
                continue

            m = re.match(
                r"^\s*(?:(?:virtual|static|explicit|inline|constexpr|friend)\s+)*"
                r"(?:[\w:<>,\s\*&]+?\s+)?(\w+)\s*\([^;{]*\)\s*(?:const\s*)?"
                r"(?:noexcept\s*)?(?:override\s*)?(?:final\s*)?;\s*$",
                line,
            )
            if not m:
                continue
            name = m.group(1)
            if name in (
                "if", "for", "while", "switch", "return", "sizeof", "Q_OBJECT",
                "Q_PROPERTY", "Q_ENUM", "using", "typedef", "static_assert",
                "friend", "public", "private", "protected", "signals", "slots",
                "operator", "emit",
            ):
                continue
            out.append((name, idx + 1, False))
        return out

    def _nested_class_names(self, body: str) -> set[str]:
        """Classes declaradas dentro de outra (Job dentro de FrameExporter)."""
        out: set[str] = set()
        depth = 0
        buf: list[str] = []
        for ch in body:
            if ch == "{":
                depth += 1
                if depth >= 2:
                    buf = []
                continue
            if ch == "}":
                depth -= 1
                continue
            if depth >= 2:
                buf.append(ch)
        # Varredura simples: class X dentro do corpo
        for m in re.finditer(r"\bclass\s+(\w+)", body):
            out.add(m.group(1))
        return out

    def _class_of(self, path: Path, header: Path) -> str | None:
        """
        A classe que este .cpp implementa e a classe principal do .h
        correspondente.

        Nao serve procurar por 'Classe::' no .cpp: qualquer 'std::algo' ou
        'Transform::algo' comecado na primeira coluna casaria, e o guard
        acusaria metodo de outra classe como faltando. A classe do header e a
        fonte da verdade.
        """
        htext = read(header)
        m = re.search(r"\bclass\s+(\w+)\s*(?::|\{)", htext)
        if not m:
            return None
        cls = m.group(1)
        ctext = read(path)
        if not re.search(rf"\b{re.escape(cls)}::\w+\s*\(", ctext):
            return None
        return cls

    def _class_body(self, htext: str, cls: str) -> str:
        """
        O conteudo entre as chaves da classe. Sem isso, o guard le metodos
        estaticos de outras classes declaradas no mesmo arquivo e exige que
        estejam definidos no .cpp errado.
        """
        m = re.search(rf"\bclass\s+{re.escape(cls)}\s*(?::[^{{]*)?\{{", htext)
        if not m:
            return htext
        start = m.end() - 1
        depth = 0
        for i in range(start, len(htext)):
            if htext[i] == "{":
                depth += 1
            elif htext[i] == "}":
                depth -= 1
                if depth == 0:
                    return htext[start: i + 1]
        return htext[start:]

    # -- regra: membro ausente -----------------------------------------------

    def check_members(self) -> None:
        for path in self.sources:
            if path.suffix != ".cpp":
                continue
            header = path.with_suffix(".h")
            if not header.is_file():
                continue
            cls = self._class_of(path, header)
            if cls is None:
                continue

            ctext = strip_comments_and_strings(read(path))
            htext = strip_comments_and_strings(read(header))

            # O que a classe (corpo + bases) declara, e o que os headers que
            # ela inclui trazem. Sem olhar para os incluidos, todo membro
            # herdado de uma classe-base vira falso positivo.
            # Um membro comeca com m_ seguido de letra minuscula.
            #
            # O lookahead nao pode vir depois de \w*: com quantificador guloso
            # seguido de (?!\s*\(), o motor volta uma caractere por vez e
            # devolve o MENOR match que satisfaz a assercao - "m_project"
            # virava "m_projec". A verificacao e feita no codigo, depois de
            # casar o identificador inteiro.
            membro_re = re.compile(r"\bm_[a-z]\w*")
            def membros(texto: str) -> set[str]:
                out = set()
                for m in membro_re.finditer(texto):
                    tail = texto[m.end():].lstrip()
                    if tail.startswith("("):
                        continue    # e um metodo, nao um dado
                    out.add(m.group(0))
                return out

            declared = membros(htext)
            for m in re.finditer(r'#include\s+"([^"]+)"', htext):
                for base in (header.parent / m.group(1), self.root / "src" / m.group(1)):
                    if base.is_file():
                        declared |= membros(strip_comments_and_strings(read(base)))

            for m in membro_re.finditer(ctext):
                name = m.group(0)
                if ctext[m.end():].lstrip().startswith("("):
                    continue
                if name in declared:
                    continue
                self.add(Finding(
                    rule="member-missing",
                    file=self._rel(path),
                    line=line_of(ctext, m.start()),
                    message=f"usa {name}, que {cls} nao declara nem herda",
                ))

    # -- regra: aridade de connect -------------------------------------------

    def check_connect(self) -> None:
        for path in self.sources:
            if path.suffix != ".cpp":
                continue
            ctext = strip_comments_and_strings(read(path))
            for m in re.finditer(r"connect\s*\(", ctext):
                # Extrai a chamada equilibra
                i = m.end() - 1
                depth = 0
                j = i
                while j < len(ctext):
                    if ctext[j] == "(":
                        depth += 1
                    elif ctext[j] == ")":
                        depth -= 1
                        if depth == 0:
                            break
                    j += 1
                call = ctext[m.start(): j + 1]

                slots = re.findall(r"&[\w:]+::(\w+)\s*(?:,|\))", call)
                if not slots:
                    continue
                slot_name = slots[-1]
                slot_args = self._args_of_lambda_or_slot(call, slot_name)
                sig_args = None
                for s in re.findall(r"&[\w:]+::(\w+)\s*(?:,)", call)[:-1]:
                    if s != slot_name:
                        sig_args = self._arity_from_header(slot_name, call)
                        break
                if sig_args is None:
                    continue
                if slot_args is not None and sig_args != slot_args:
                    self.add(Finding(
                        rule="signal-slot",
                        file=self._rel(path),
                        line=line_of(ctext, m.start()),
                        message=(
                            f"connect para {slot_name}: sinal tem {sig_args} "
                            f"argumento(s) e o slot {slot_args}"
                        ),
                    ))

    def _arity_from_header(self, slot_name: str, call: str) -> int | None:
        owner = re.search(r"&(\w+)::" + re.escape(slot_name), call)
        if not owner:
            return None
        target = self._class_index.get(owner.group(1))
        if not target:
            return None
        htext = strip_comments_and_strings(read(target[0]))
        m = re.search(
            rf"\b{re.escape(slot_name)}\s*\(([^)]*)\)", htext
        )
        if not m:
            return None
        args = m.group(1).strip()
        if not args or args == "void":
            return 0
        # Conta virgulas no nivel superior
        depth = 0
        count = 1
        for c in args:
            if c in "(<[":
                depth += 1
            elif c in ")>]":
                depth -= 1
            elif c == "," and depth == 0:
                count += 1
        return count

    def _args_of_lambda_or_slot(self, call: str, slot_name: str) -> int | None:
        # Procura "slot_name(this" ou "slot_name, this" dentro da chamada
        m = re.search(
            re.escape(slot_name) + r"\s*(?:\(|,\s*this)", call
        )
        if not m:
            return None
        tail = call[m.end():]
        if tail.lstrip().startswith("("):
            depth = 1
            k = tail.index("(") + 1
            start = k
            while k < len(tail) and depth:
                if tail[k] == "(":
                    depth += 1
                elif tail[k] == ")":
                    depth -= 1
                k += 1
            args = tail[start: k - 1].strip()
            if not args:
                return 0
            depth = 0
            count = 1
            for c in args:
                if c in "(<[":
                    depth += 1
                elif c in ")>]":
                    depth -= 1
                elif c == "," and depth == 0:
                    count += 1
            return count
        return 0

    # -- regra: alvo CMake inexistente ---------------------------------------

    def check_cmake_targets(self) -> None:
        """
        Dependencia de link que ninguem declara.

        A versao anterior so procurava nomes crus (lumina_core) e ignorava os
        nomespaced, que e como este projeto escreve tudo (lumina::core). Um
        alvo renomeado por engano passava batido: era o que faltava checar.
        """
        for path in self.cmake_files:
            text = read(path)
            for target, body, off in link_blocks(text):
                deps: list[tuple[str, int]] = []
                # nomespaced: lumina::core   (o \w+ antes dos :: e o prefixo)
                for d in re.finditer(r"\b([A-Za-z_]\w*)::([A-Za-z_]\w*)", body):
                    deps.append((d.group(0), d.start()))
                # nomes crus: lumina_core
                for d in re.finditer(r"(?<![:\w])(lumina_[A-Za-z_]\w*)", body):
                    deps.append((d.group(1), d.start()))

                for name, off in deps:
                    prefix, _, bare = name.rpartition("::")
                    if not prefix:
                        continue
                    if prefix in ("Qt6", "Qt5"):
                        # Nao vem de add_library: vem do find_package. Aqui o
                        # que interessa e se o COMPONENTS pediu este modulo.
                        if not self._qt_component_requested(prefix, bare):
                            lineno = line_of(text, off) + body[:off].count("\n")
                            self.add(Finding(
                                rule="cmake-target",
                                file=self._rel(path),
                                line=lineno,
                                message=(
                                    f"'{target}' linka '{prefix}::{bare}', "
                                    "mas find_package nao pediu este modulo"
                                ),
                                fixable=True,
                                fix_hint=f"acrescentar {bare} aos COMPONENTS do find_package({prefix})",
                            ))
                        continue
                    if prefix != "lumina":
                        continue          # biblioteca externa de nome proprio
                    if self._target_exists(name):
                        continue
                    lineno = line_of(text, off) + body[:off].count("\n")
                    self.add(Finding(
                        rule="cmake-target",
                        file=self._rel(path),
                        line=lineno,
                        message=f"'{target}' linka '{name}', que ninguem declara",
                        fixable=True,
                        fix_hint="corrigir para um alvo existente ou remover",
                    ))

    def _qt_component_requested(self, prefix: str, module: str) -> bool:
        """O find_package do projeto pediu este modulo do Qt?"""
        any_ = False
        for mods in self._find_modules.values():
            if module in mods:
                return True
            any_ = True
        # Sem COMPONENTS explicito o Qt traz o pacote inteiro e nao ha o que
        # cobrar. (Nao confundir com find_package sem argumentos nenhum.)
        return not any_

    def _target_exists(self, name: str) -> bool:
        """Existe como add_library/add_executable, direto ou via ALIAS."""
        bare = name.rpartition("::")[2]
        return (
            name in self._cmake_targets
            or bare in self._cmake_targets
            or name in self._cmake_aliases
            or bare in self._cmake_aliases
        )


    # -- regra: fonte fora do CMake ------------------------------------------

    def check_cmake_sources(self) -> None:
        """
        .cpp do mesmo diretorio que nao esta listado no CMakeLists daquele
        diretorio.

        Comparar com o CMakeLists da raiz dava 54 falsos positivos: os arquivos
        de src/ui pertencem a src/ui/CMakeLists.txt, nao ao CMakeLists.txt da
        raiz. Comparar so com o CMakeLists vizinho e a checagem que vale.
        """
        for path in self.cmake_files:
            if path.parent.name in ("cmake", ".github", "build"):
                continue
            text = read(path)
            if "add_library" not in text and "add_executable" not in text:
                continue
            listed = {
                p.replace("\\", "/")
                for p in re.findall(r"([\w./\\-]+\.cpp)", text)
            }

            for src in sorted(path.parent.glob("*.cpp")):
                if ".git" in src.parts or "build" in src.parts:
                    continue
                if src.name.startswith(("moc_", "qrc_")):
                    continue
                if src.name in listed:
                    continue
                self.add(Finding(
                    rule="cmake-source",
                    file=self._rel(path),
                    line=line_of(text, text.find("add_")),
                    message=f"{src.name} existe mas nao esta listado neste CMakeLists",
                    fixable=True,
                    fix_hint="acrescentar ao add_library/add_executable",
                ))


    # -- regra: sinal do Qt inexistente --------------------------------------

    KNOWN_QT = {
        "QWidget": set(),
        "QDialog": set(),
        "QFrame": {"frameChanged"},
        "QLineEdit": {"textChanged", "textEdited", "returnPressed", "editingFinished"},
        "QTextEdit": {"textChanged", "textChanged"},
        "QPlainTextEdit": {"textChanged"},
        "QAbstractSlider": {"valueChanged", "sliderMoved", "sliderPressed", "sliderReleased", "rangeChanged", "actionTriggered"},
        "QSlider": {"valueChanged", "sliderMoved", "sliderPressed", "sliderReleased", "actionTriggered"},
        "QDial": {"valueChanged", "sliderMoved"},
        "QComboBox": {"currentIndexChanged", "currentTextChanged", "editTextChanged", "activated", "highlighted"},
        "QCheckBox": {"toggled", "stateChanged", "clicked"},
        "QRadioButton": {"toggled"},
        "QSpinBox": {"valueChanged", "textChanged", "editingFinished"},
        "QDoubleSpinBox": {"valueChanged", "textChanged", "editingFinished"},
        "QToolButton": {"clicked", "toggled", "pressed", "released", "doubleClicked"},
        "QPushButton": {"clicked", "pressed", "released", "toggled", "doubleClicked"},
        "QLabel": {"linkActivated", "linkHovered"},
        "QAbstractButton": {"clicked", "pressed", "released", "toggled", "doubleClicked"},
        "QTabWidget": {"currentChanged", "tabBarClicked"},
        "QTabBar": {"currentChanged", "tabBarClicked", "tabMoved", "tabCloseRequested"},
        "QDockWidget": {"visibilityChanged", "dockLocationChanged", "featuresChanged", "topLevelChanged", "tabCloseRequested"},
        "QMenu": {"aboutToShow", "aboutToHide", "triggered"},
        "QTimer": {"timeout"},
        "QAction": {"triggered", "toggled", "changed", "enabledChanged", "checkableChanged", "visibleChanged"},
        "QTreeWidget": {"itemClicked", "itemDoubleClicked", "itemActivated", "itemChanged", "itemSelectionChanged", "currentItemChanged"},
        "QTreeWidgetItem": {"itemChanged"},
        "QListWidget": {"itemClicked", "itemDoubleClicked", "itemChanged", "itemSelectionChanged"},
        "QSlider": {"valueChanged", "sliderMoved", "sliderPressed", "sliderReleased", "actionTriggered"},
        "QFileSystemModel": {"directoryLoaded", "fileRenamed", "rowInserted", "rowRemoved"},
        "QOpenGLWidget": {"aboutToBeDestroyed", "contextAboutToBeDestroyed", "frameSwapped", "paintGL"},
        "QProgressBar": {"valueChanged"},
    }

    def check_qt_signals(self) -> None:
        for path in self.sources:
            if path.suffix != ".cpp":
                continue
            code = strip_comments_and_strings(read(path))
            for m in re.finditer(r"&(Q\w+)::(changed|toggled|clicked|valueChanged)\b", code):
                cls, sig = m.group(1), m.group(2)
                known = self.KNOWN_QT.get(cls)
                if known is None:
                    continue
                if sig in known:
                    continue
                self.add(Finding(
                    rule="qt-signal",
                    file=self._rel(path),
                    line=line_of(code, m.start()),
                    message=f"{cls} nao tem o sinal '{sig}' (verifique a assinatura)",
                ))

    def _rel(self, path: Path) -> str:
        try:
            return str(path.relative_to(self.root)).replace("\\", "/")
        except ValueError:
            return str(path)

    # -- execucao -------------------------------------------------------------

    def run(self) -> None:
        # A regra de include e optativa: a include transitiva de headers do Qt
        # e da biblioteca padrao e a regra no mundo real, e reportar 524
        # falsos positivos esconde as quatro regras que tem precisao real.
        if self.want_includes:
            self.check_includes()
        self.check_qt_modules()
        self.check_decl_def()
        self.check_members()
        self.check_connect()
        self.check_qt_signals()
        self.check_cmake_targets()
        self.check_cmake_sources()


def _unified_tail(before: str, after: str) -> str:
    return before + after


def _patch_add_source(cmake_path: Path, src: Path, rel_src: str) -> str:
    text = read(cmake_path)
    name = src.name
    for m in re.finditer(r"add_(?:library|executable)\(\s*[\w.]+([\s\S]*?)\n\)", text):
        at = line_of(text, m.start(1)) - 1
        lines = text.splitlines()
        entry = f"    {name}"
        if name in lines:
            return ""
        lines.insert(at, entry)
        return (
            f"--- a/{cmake_path.name}\n+++ b/{cmake_path.name}\n"
            f"@@ -{at + 1},0 +{at + 1},1 @@\n+{entry}\n"
        ) + _unified_tail(
            "".join(f"{ln}\n" for ln in lines[:at]),
            "".join(f"{ln}\n" for ln in lines[at:]),
        )
    return ""


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

RULES = [
    "decl-without-def", "qt-module", "cmake-target", "qt-signal", "member-missing",
    "include-missing", "def-without-decl", "signal-slot", "cmake-source",
]

FIXABLE = {"include-missing", "qt-module", "cmake-source"}


def find_qt_root(explicit: str | None) -> Path | None:
    if explicit:
        p = Path(explicit)
        return p if p.is_dir() else None
    env = os.environ.get("QT_ROOT")
    if env:
        p = Path(env)
        if p.is_dir():
            return p
    # Probe local do ambiente de desenvolvimento
    for base in (
        Path.home() / "AppData/Local/Temp/opencode/qt-probe/6.8.3/msvc2022_64",
        Path("C:/Qt/6.8.3/msvc2022_64"),
        Path("C:/Qt/6.8.3/msvc2019_64"),
    ):
        if (base / "include").is_dir():
            return base
    return None


def main() -> int:
    # O console do Windows usa cp1252 e trava ao imprimir '→'. Forcar UTF-8
    # aqui evita que o guard morra no meio do relatorio, que e exatamente o
    # que aconteceu na primeira versao.
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            pass

    ap = argparse.ArgumentParser(description="Verificador do Lumina")
    ap.add_argument("--fix", action="store_true", help="corrige o que for mecanico")
    ap.add_argument("--quiet", action="store_true", help="so o resumo")
    ap.add_argument("--rule", help=f"roda so uma regra: {', '.join(RULES)}")
    ap.add_argument("--qt", help="caminho do Qt instalado (para conhecer os modulos)")
    ap.add_argument("--root", default=".", help="raiz do projeto")
    ap.add_argument("--includes", action="store_true", help="inclui a regra include-missing (ruidosa: include transitiva)")
    args = ap.parse_args()

    root = Path(args.root).resolve()
    if not (root / "CMakeLists.txt").is_file():
        print(f"guard: {root} nao parece a raiz do projeto", file=sys.stderr)
        return 2

    load_qt_knowledge(find_qt_root(args.qt))

    rules = {args.rule} if args.rule else None
    if rules and args.rule not in RULES:
        print(f"guard: regra desconhecida '{args.rule}'", file=sys.stderr)
        return 2

    guard = Guard(root, None, rules, want_includes=args.includes)
    guard.run()

    if not args.quiet:
        by_rule: dict[str, list[Finding]] = defaultdict(list)
        for f in guard.findings:
            by_rule[f.rule].append(f)

        print()
        for rule in RULES:
            items = by_rule.get(rule)
            if not items:
                continue
            print(f"\033[1m{rule}\033[0m ({len(items)})")
            for f in sorted(items, key=lambda x: (x.file, x.line)):
                mark = "\033[32m[corrigivel]\033[0m" if f.fixable else "\033[31m[manual]\033[0m"
                print(f"  {SEV.get(rule, '?'):7} {f.file}:{f.line}  {mark}")
                print(f"          {f.message}")
                if f.fix_hint and f.fixable:
                    print(f"          \u2192 {f.fix_hint}")
        print()

    erros = [f for f in guard.findings if SEV.get(f.rule) == "ERRO"]
    avisos = [f for f in guard.findings if SEV.get(f.rule) != "ERRO"]

    if args.fix:
        applied = _apply_fixes(guard)
        if applied:
            print(f"corrigidos {applied} problema(s) automatico(s). Revise com git diff.\n")

    print(
        f"{len(erros)} erro(s) bloqueante(s), {len(avisos)} aviso(s).",
        f"{len(guard.findings)} achado(s) no total.",
    )
    return 1 if erros else 0


def _apply_fixes(guard: Guard) -> int:
    """Aplica os patches, um por vez, reescrevendo o arquivo."""
    por_arquivo: dict[str, list[Finding]] = defaultdict(list)
    for f in guard.findings:
        if f.fixable and f.fix:
            por_arquivo[f.file].append(f)

    aplicados = 0
    for rel, findings in por_arquivo.items():
        path = guard.root / rel
        text = read(path)
        original = text
        for f in sorted(findings, key=lambda x: -x.line):
            if f.rule == "include-missing":
                header = re.search(r"#include <(\w+)>", f.message or "")
                if not header:
                    continue
                lines = text.splitlines()
                at = find_include_insert_point(lines)
                if any(ln.strip() == f"#include <{header.group(1)}>" for ln in lines):
                    continue
                lines.insert(at, f"#include <{header.group(1)}>")
                text = "\n".join(lines) + ("\n" if original.endswith("\n") else "")
                aplicados += 1
            elif f.rule == "qt-module":
                m = re.search(r"Qt(\w+) e", f.message or "")
                module = m.group(1) if m else None
                if not module:
                    m2 = re.search(r"Qt(\w+)", f.message or "")
                    module = m2.group(1) if m2 else None
                if not module:
                    continue
                # 1) find_package COMPONENTS
                fm = re.search(
                    r"(find_package\(Qt6[^)]*?COMPONENTS\s+)([^)]*?)(\))",
                    text,
                    re.S,
                )
                if fm:
                    comps = fm.group(2).split()
                    if module not in comps:
                        text = (
                            text[: fm.start(2)]
                            + fm.group(2).rstrip() + f"\n    {module}"
                            + text[fm.end(2):]
                        )
                        aplicados += 1
                # 2) target_link_libraries
                tm = re.search(
                    r"(target_link_libraries\(\s*\w+[\s\S]*?Qt6::)(\w+)([\s\S]*?\n\))",
                    text,
                )
                if tm and module not in tm.group(2):
                    text = text[: tm.start()] + (
                        f"{tm.group(1)}{tm.group(2)}\n        Qt6::{module}"
                        f"{tm.group(3)}"
                    ) + text[tm.end():]
        if text != original:
            path.write_text(text, encoding="utf-8")
    return aplicados


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:  # noqa: BLE001
        print(f"guard: erro interno: {exc}", file=sys.stderr)
        sys.exit(2)
