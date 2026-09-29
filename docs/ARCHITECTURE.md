# Arquitetura

Este documento explica as decisoes que moldaram o codigo. O objetivo e que
alguem possa mudar o projeto sem quebrar as invariantes que nao estao
explicitas em nenhum lugar.

## O problema

x que reunisse After Effects, Premiere e DaVinci Resolve,
fosse o mais completo possivel, tivesse interface facil e rodasse num PC
modesto, em um executavel portatil.

Esses quatro requisitos entram em conflito, e as escolhas abaixo sao o resultado
de resolver esses conflitos em uma ordem especifica:

1. **"o mais completo de todos"** renderiza uma lista enorme de recursos.
2. **"em PC modesto"** limita memoria, CPU e GPU disponiveis.
3. **"interface facilmente"** exige que cada recurso tenha uma decisao de uso
   *antes* de existir.
4. **"portatil"** limita o que pode ser empacotado.

A ordem de resolucao: 2 define 1, 3 define 1, 4 filtra o resto. Um recurso
que exija 200 MB de RAM por si so nao entra, mesmo que "todos os editores"
o tenham. Um recurso cujo uso nao caiba em um clique fica adiado, porque
interface que exige estudo nao e interface facil.

## Decisoes centrais

### C++ com Qt Widgets, nao Electron nem webview

A decisao mais consequente do projeto. Um editor de video precisa de acesso
direto a GPU com previsibilidade de latencia; o preco e trabalho de infraestrutura
de interface.

O que ganhamos: binario de ~40 MB sem runtime de JavaScript, latencia de
pintura previsivel, e um executavel que abre em menos de um segundo mesmo em
disco lento (importante numa maquina modesta).

O que perdemos: iteracao mais lenta em interface, e mais codigo para
componentes que numa webview seriam gratis.

**O que torna isso viavel:** todos os paineis sao desenhados com `QPainter` em
um unico widget, e nao compostos por widgets filhos. Uma timeline com 500 clips
e um widget so. Isso era obrigatorio: a alternativa (um widget por clip) trava
assim que o projeto cresce.

### Grafo de nos unificando as tres referencias

After Effects organiza em camadas; Fusion e Resolve em nos. Sao o mesmo
conceito com nomes diferentes, e o modelo do projeto adota o de nos porque
ele e mais geral: **um no e simultaneamente uma camada e um operador**.

- Um `Solid` e uma camada (fonte, sem entrada).
- Um `Merge` e um operador (combina duas entradas).
- Um `fx.blur` e uma camada com efeito (uma entrada, uma saida).

Dessa forma, o mesmo motor renderiza uma pilha de camadas do estilo AE e um
grafo nao-linear do estilo Nuke, sem dois caminhos de codigo. `NodeGraph`
aceita as duas formas porque so sabe orderar topologicamente.

`Composition` e um grafo nomeado; `Timeline` e uma sequencia de clips que
apontam para composicoes. Aninhar e por referencia (`comp.reference`), nao por
hierarquia: e assim que o Premiere faz pre-composicao e o AE faz pre-comp.

### Espaco linear com conversao em um unico ponto

Toda a composicao e processada em **linear**, e a conversao para o display
acontece em um no (`color.viewTransform`).

A alternativa - processar em sRGB, como faz muito software - e mais simples e
produz resultados errados de formas que so aparecem em producao:

- Blur em sRGB produz halo nas bordas de alto contraste
- Modos de mesclagem como Overlay ficam com resposta errada nas sombras
- Composicao de varios efeitos acumula em gamma e estoura os claros

A conversao em um unico ponto tambem torna o resultado **independente de onde
ele e aplicado**: o mesmo grafo produz o mesmo pixel no preview e no arquivo
exportado, porque o pipeline e o mesmo.

### Expressoes proprias em vez de Lua

After Effects usa Lua. Aqui o motor e proprio: lexer, parser e interpretador
em ~1000 linhas.

Por que nao Lua:

- ~500 KB de biblioteca a menos no binario
- Controle sobre avaliacao: `wiggle` e deterministico por construcao, sem
  estado entre frames
- Mensagens de erro em portugues, com linha e coluna

Por que funciona: a linguagem e pequena de proposito. Numeros, texto, vetores,
operadores, funcoes e acesso a propriedades. Nao tem metaprogramacao, e nao
precisa.

**O detalhe que importa:** `wiggle` usa ruido de valor com hash, sem estado
entre frames. Renderizar o mesmo frame duas vezes produz o mesmo pixel, e
exportar em paralelo continua reproduzivel. Um `wiggle` com estado escondido
faria o preview e a exportacao divergirem, e o usuario perderia a confianca no
resultado.

### Shaders proprios, um por no

Cada no tem um fragment shader GLSL em `resources/shaders/`. Sao compilados
no executavel via recurso Qt, entao a pasta portavel nao precisa de arquivos ao
lado.

O modo de mesclagem e o caso interesante: 26 modos vivem em **um** shader com
um `switch` por uniforme, em vez de 26 programas. Trocar de modo com keyframe
nao recompila nada, e o cache de programas fica pequeno.

### Interface gerada a partir da descricao

`NodeTypeDesc` declara propriedades com nome, tipo, faixa, controle de UI e
dica. O inspector, a biblioteca e a serializacao leem essa mesma descricao.

Consequencia pratica: **um no novo aparece na interface sem escrever UI**. E o
que torna a biblioteca crescer sem a interface virando o gargalo.

### Modo leve como decisao de arquitetura, nao como recurso

"Rodar em PC modesto" seria um slider de qualidade se fosse tratado como
opcao. Ele e tratado como limite de projeto, e por isso existe um botao e um
atalho.

Modo leve significa:

- Previa em 50% (4x menos fragmento)
- Cache de video em 24 MB em vez de 96 MB
- Proxies usados quando existem
- Sem dither nos gradientes

E **automatico**: `PlaybackController` conta quadros perdidos e ativa o modo
sozinho quando a reproducao nao se mantem. O usuario e avisado, nao corrigido
em silencio.

## Invariantes

Estas regras sustentam o resto. Quebrar uma delas produz bug dificil de
diagnosticar.

### 1. Espaco de cor de trabalho e sempre linear

Shaders nunca recebem nem devolvem valores em gamma. `solid.frag` converte a
cor da propriedade de sRGB para linear; `view_transform.frag` faz a conversao
de volta, uma unica vez.

### 2. Alpha e premultiplicado dentro do pipeline

Toda textura intermediaria tem `rgb` premultiplicado por `a`. A conversao
acontece na entrada (o no converte straight para premultiplicado) e na saida
(na tela). Sem isso, um merge divide por zero em `a = 0` e produz ruido.

### 3. `NodeId` nao identifica um no sozinho

Cada grafo numera seus nos a partir de 1, entao o id 1 existe em todas as
composicoes. Qualquer API que receba um id de no precisa tambem do `CompId`.
A UI trabalha com o par.

### 4. Ids nunca sao reutilizados

`NodeGraph` gera ids monotonicos e nunca reaproveita. Desfazer um
`AddNodeCommand` e um `RemoveNodeCommand` em sequencia precisa deixar o grafo
no mesmo estado, e isso so funciona se o id removido nao voltar a ser dado a
outro no.

### 5. Expressao invalida cai para a curva, nunca para zero

Se uma expressao falha ao compilar, `Property::evaluate` usa a curva de
keyframes. Retornar zero esconderia o conteudo do no sem explicacao.

### 6. O registro de nos e a unica fonte de verdade

`NodeRegistry` responde "quais propriedades este no tem" para a UI, o
compositor e a serializacao. Nenhum dos tres mantem lista propria.

### 7. Tudo que aloca memoria de video passa pelo pool

`RenderTargetPool` recicla alvos do tamanho da composicao. Alocar um FBO por
passe e por frame e o que derruba um editor em maquina fraca.

## Onde o custo esta

Para quem for mexer no nucleo, os pontos caros conhecidos:

| Onde | Custo | Por que |
|------|-------|---------|
| `blur.frag` | 13 amostras x 2 passadas | Raio grande em 4K. Reduzir `u_quality` e a saida. |
| `glow.frag` | 3 niveis x 5 amostras | Cada nivel custa um desfoque. |
| `curves.frag` | Busca linear nos pontos | Maximo de 8 pontos por canal, entao e irrelevante. |
| `view_transform.frag` | 3 `pow` por pixel | Inevitavel para gamma. |
| Ordenacao topologica | Recursiva, por frame | Com cache de revisao, so quando o grafo muda. |
| `evaluate()` de Property | Busca binaria nas keys | Log(n), com n pequeno. |

Se algum dia algum destes aparecer no perfil, o caminho e o mesmo: cache por
`(no, tempo)` com invalidacao por revisao do grafo, e nao otimizar o codigo
quente.

## Limitacoes conhecidas

- **Sem OpenGL:** o compositor exige 3.3 core. Maquinas muito antigas (pre-2013
  sem driver) nao rodam. O caminho de fallback seria um rasterizador em CPU, e
  ele nao existe.
- **Um contexto por thread:** `FrameExporter::Job` cria o proprio contexto
  offscreen. Exportar varios videos em paralelo exige varias threads, nao
  varias instancias do mesmo job.
- **Sem proxy automatico:** `generateProxy` e a interface; nada a chama ainda.
- **Sem desfazer na timeline:** o desfazer cobre o grafo de nos, nao o corte de
  clip. As acoes existem, o agrupamento em macro ainda nao.

## Decisoes que valem revisar

Coisas que foram decididas com a informacao disponivel e que valem uma
segunda opiniao quando o projeto crescer:

1. **JSON para o arquivo de projeto.** Legivel e diferencial, e o unico jeito
   de um format de projeto nao bloquear a mergers de duas pessoas. Custa ~1 ms
   por MB e duplica o tamanho em relacao a binario. Se projeto passar de 50 MB
   com um volume grande de keyframe, um backend binario com um `.json` de fallback
   resolve.

2. **Thread unica para o grafo.** `Compositor` renderiza na thread que chama.
   Isso elimina problema de sincronizacao inteiro ao custo de nao explorar
   varios nucleos. Para grafos com muitos nos independentes, paralelizar por
   nivel do topo-logico seria o proximo passo.

3. **Sliders com faixa limitada a -1000..1000.** Um angulo de 720 graus
   ultrapassa o slider e so pode ser digitado no campo. Bug conhecido, nao
   resolvido de proposito: mudar agora e mexer em codigo que ainda nao
   demonstrou precisar.
