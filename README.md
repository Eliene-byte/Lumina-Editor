# Lumina

Editor de video portatil que junta o que costuma exigir tres programas
separados: **composicao por nos** (After Effects / Fusion), **linha do tempo
multipista** (Premiere) e **correcao de cor com gerenciament de cor**
(DaVinci Resolve).

Um unico executavel. Sem instalador, sem registro, sem permissao de
administrador: extrai o zip em qualquer pasta e roda.

---

## O que ja funciona

Esta e a base do compositor por nos, com o pipeline completo de render. O
que existe hoje:

**Nucleo de composicao**
- Grafo de nos com ciclos detectados e ordem topologica garantida
- Propriedades animaveis com keyframes, interpolacao linear/Bezier/degrau e
  expressoes no estilo After Effects (`wiggle`, `loopOut`, `time`, `thisComp`,
  `Math.*`)
- 21 tipos de no: fontes (solido, ruido procedural, formas geometricas),
  correcao de cor (brilho/contraste, matiz/saturacao, curvas, lift/gamma/gain,
  LUT 3D), filtros (desfoque, brilho, deslocamento, vinheta, remocao de fundo,
  matte), mesclagem (Merge com 26 modos) e tempo
- Pipeline em espaco linear, com a conversao para display em um unico no -
  o que faz blend e blur se comportarem corretamente

**Interface**
- Editor de grafos com cabos em curva, arraste de cabo existente, encaixe na
  grade e arrastar-soltar da biblioteca
- Inspector gerado a partir da descricao do no (um no novo aparece na interface
  sem nenhuma linha de codigo de UI)
- Linha do tempo multipista com trim, corte, ripple delete, marcadores de
  keyframe e arrastar para reordenar
- Visualizador OpenGL com conta-gotas, areas seguras e modo antes/depois
- Biblioteca de nos com busca que tambem casa por propriedade
- Desfazer/refazer por comando, com pilha de acoes

**Engenharia**
- Modo leve automatico: quando a reproducao perde frames, a previa cai para
  50% e o cache encolhe sozinho
- Exportacao headless (`--render`), sem abrir janela
- Formato de projeto JSON legivel e versionado
- CI que compila, roda os testes e gera o .zip portavel a cada push

## O que ainda nao existe

Para ser claro sobre onde o projeto esta:

- **Decodificacao de video.** A interface existe, o backend e um stub honesto.
  O FFmpeg esta ligado no CMake, mas a implementacao de `VideoSource` nao foi
  escrita. ate la, o grafo de nos funciona inteiro - so nao ha imagem.
- **Codificacao de video.** `Encoder` e uma interface sem implementacao.
- **Audio.** Modelo de dados existe; mixer, forma de onda e medidores nao.
- **Rastreamento, mascaras com bezier editaveis, texto, 3D.**
- **Aba de color grading com scopes** (waveform, vectorscope, histograma).
- **Grupos de nos** (os comandos existem e avisam que nao estao implementados,
  em vez de fingir que funcionam).

O que existe e real e testado. O que falta esta listado acima em vez de
escondido.

---

## Compilando

### So o aplicativo (recomendado)

```
git clone <url>
cd lumina
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

O executavel fica em `build/src/app/Release/lumina.exe`.

### Testes

```
build/tests/Release/lumina_tests.exe
```

### Sem FFmpeg

```
cmake -S . -B build -DLUMINA_ENABLE_FFMPEG=OFF
```

Compila tudo sem FFmpeg. O grafo de nos, a interface e a exportacao de imagem
funcionam; apenas midia externa fica indisponivel, e o editor diz isso na barra
de status em vez de falhar em silencio.

### O CI gera o portatil

Push em `main` dispara `.github/workflows/build.yml`, que compila, roda os
testes, monta a pasta com `windeployqt` e publica o `.zip` como artefato. Uma
tag `v*` publica tambem uma release.

## Uso

```
lumina                     # abre vazio
lumina projeto.lumina      # abre um projeto
lumina --render saida.png  # renderiza um quadro e sai, sem janela
lumina --modo-leve         # abre em modo leve
```

Atalhos principais: **Espaco** reproduz, **Ctrl+L** liga o modo leve,
**K** divide o clip no cursor, **I/O** marcam entrada e saida.

## Arquitetura

```
src/core/     Modelo de dados: Value, Property, Node, grafo, composicao, projeto
src/expr/     Motor de expressoes (lexer, parser, interpretador)
src/gpu/      OpenGL: contexto, shaders, alvos, compositor
src/nodes/    Biblioteca de nos: o que o usuario ve na interface
src/media/    Midia: interfaces de decodificacao e codificacao
src/io/       Leitura e escrita do arquivo de projeto
src/ui/       Interface Qt
resources/    Shaders GLSL e tema, compilados no executavel
tests/        Testes, incluindo os que rodam sem GPU
```

As dependencias sao deliberadamente poucas: Qt 6 (Widgets + OpenGL) e FFmpeg
(opcional). Sem Electron, sem React, sem WebView, sem gerenciador de pacotes.
O motor de expressoes, o compositor e o formato de arquivo sao proprios, o que
mantem o binario pequeno e o comportamento previsivel.

Detalhes em [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Licenca

LGPL-3.0-or-later. O Qt e usado sob LGPL; o FFmpeg (quando presente) sob LGPL.
Nada aqui exige licenca comercial para uso ou redistribuicao.
