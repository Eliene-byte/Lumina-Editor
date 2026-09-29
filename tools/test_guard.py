"""
Teste do proprio guard: injeta bugs conhecidos e confere se ele acha.

Um verificador que nunca accuse nada tambem "passa" em tudo. Este arquivo
injeta erros de proposito num copia temporaria do projeto e falha se o guard
naos reclamar. Bug de regra e exatamente o tipo de coisa que passa batido
quando so se olha o relatorio de saida.

Uso:  python tools/test_guard.py
"""

from __future__ import annotations

import io
import re
import sys
import tempfile
from contextlib import redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import guard  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent


class Failure(Exception):
    pass


def findings(root: Path) -> list[str]:
    """Roda o guard sobre `root` e devolve as mensagens, sem sair do processo."""
    # O conhecimento do Qt vive em estado de modulo e so e carregado por
    # main(). Sem carregar aqui, QT_CLASS_TO_MODULE fica vazio e a regra de
    # modulo Qt nao tem o que cobrar - o teste passaria sem checar nada.
    guard.load_qt_knowledge(None)

    g = guard.Guard(root, None, None)
    buf = io.StringIO()
    with redirect_stdout(buf):
        g.run()
    return [f"{f.rule}: {f.message}" for f in g.findings]


def copied_project(tmp: Path) -> Path:
    """Copia so o que o guard le, para o teste nao pisar no projeto real."""
    dst = tmp / "proj"
    for sub in ("src", "tests", "resources", "cmake"):
        if (ROOT / sub).is_dir():
            _copy_tree(ROOT / sub, dst / sub)
    for name in ("CMakeLists.txt",):
        if (ROOT / name).is_file():
            (dst / name).write_text(
                (ROOT / name).read_text(encoding="utf-8"), encoding="utf-8"
            )
    return dst


def _copy_tree(src: Path, dst: Path) -> None:
    dst.mkdir(parents=True, exist_ok=True)
    for item in src.iterdir():
        if item.is_dir():
            if item.name in ("build", ".git", "__pycache__"):
                continue
            _copy_tree(item, dst / item.name)
        elif item.suffix in (".cpp", ".h", ".txt", ".glsl", ".vert", ".frag"):
            (dst / item.name).write_text(
                item.read_text(encoding="utf-8"), encoding="utf-8"
            )


def patch(root: Path, rel: str, old: str, new: str) -> None:
    path = root / rel
    text = path.read_text(encoding="utf-8")
    if old not in text:
        raise Failure(f"patch nao casou em {rel}: {old!r}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


# Cada caso: (nome, arquivos a alterar, regra esperada,substring esperada)
CASES = [
    (
        "metodo declarado sem definicao",
        [("src/ui/NodeEditorWidget.h",
          "void cancelConnection();",
          "void cancelConversion();\n    void cancelConnection();")],
        "decl-without-def",
        "cancelConversion",
    ),
    (
        "membro inexistente",
        [("src/ui/NodeEditorWidget.cpp",
          "void NodeEditorWidget::cancelConnection() {",
          "void NodeEditorWidget::cancelConnection() { m_membroFantasma = 1;")],
        "member-missing",
        "m_membroFantasma",
    ),
    (
        "modulo Qt necessario nao linkado",
        [("src/ui/CMakeLists.txt",
          "        Qt6::OpenGLWidgets\n",
          "")],
        "qt-module",
        "OpenGLWidgets",
    ),
    (
        "alvo renomeado por engano",
        [("src/ui/CMakeLists.txt",
          "        lumina::nodes\n",
          "        lumina::nos\n")],
        "cmake-target",
        "lumina::nos",
    ),
    (
        "modulo Qt linkado mas nao pedido em COMPONENTS",
        [("CMakeLists.txt", "    OpenGLWidgets\n", "")],
        "cmake-target",
        "OpenGLWidgets",
    ),
    (
        "cpp novo fora do CMake",
        # so cria o arquivo: a regra e "existe mas nao esta listado"
        [],
        "cmake-source",
        "ArquivoNovo.cpp",
    ),
]


def main() -> int:
    failures: list[str] = []
    baseline_count = 0

    for name, patches, rule, needle in CASES:
        with tempfile.TemporaryDirectory() as tmp:
            proj = copied_project(Path(tmp))

            if rule == "cmake-source":
                # este caso precisa que o arquivo exista de verdade
                (proj / "src/ui/ArquivoNovo.cpp").write_text(
                    "// vazio de proposito\n", encoding="utf-8"
                )
            for rel, old, new in patches:
                patch(proj, rel, old, new)

            got = findings(proj)
            hit = [g for g in got if g.startswith(rule + ":") and needle in g]

            if not hit:
                failures.append(
                    f"{name}: guard nao achou. esperava {rule} contendo {needle!r}."
                    f"\n    o que saiu ({len(got)} achados):"
                    + "".join(f"\n      {g}" for g in got[:5])
                )
                print(f"FALHOU  {name}")
            else:
                print(f"ok      {name}")
                print(f"        -> {hit[0]}")

    # Baseline: sem nenhuma injecao, o projeto tem de estar limpo. Se nao
    # estiver, ou os testes acima nao valem nada, ou ha bug novo de verdade.
    with tempfile.TemporaryDirectory() as tmp:
        proj = copied_project(Path(tmp))
        clean = findings(proj)
        baseline_count = len(clean)
        if clean:
            failures.append(
                f"baseline: projeto sem injecao ja tem {len(clean)} achado(s):"
                + "".join(f"\n      {c}" for c in clean[:20])
            )
        else:
            print("ok      baseline limpo (0 achados)")

    print()
    if failures:
        for f in failures:
            print("ERRO:", f)
        print(f"\n{len(failures)} teste(s) falharam.")
        return 1
    print(f"todos os {len(CASES)} testes do guard passaram")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
