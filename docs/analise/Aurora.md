# Relatório de análise: `Aurora.xex`

> Escopo: análise estática, sem descriptografia, sem execução e sem
> modificação do original. Dados brutos em [`dados/Aurora.header.json`](dados/Aurora.header.json),
> gerados por [`tools/xex/xex_info.py`](../../tools/xex/xex_info.py).

## 1. Identificação

| Campo | Valor |
|---|---|
| Arquivo | `references/xex/original/Aurora.xex` (12.333.056 bytes) |
| SHA-256 | `583bcd442d8017d6fcb2645b93cda987f4c0a43a688b652d7364ccaedaeefa9f` |
| Formato | XEX2, módulo `TITLE` (aplicativo, não DLL) |
| Nome PE original | `Aurora.exe` |
| Data de link do PE | 2021-11-10 21:53:46 UTC (compatível com a série 0.7b do Aurora) |
| Desenvolvedor | Phoenix (antiga Team FSD), segundo a organização [XboxUnity no GitHub](https://github.com/XboxUnity) |
| Title ID | ausente (não há cabeçalho `EXECUTION_INFO`) |
| Região / mídias | todas as regiões; tipos de mídia permitidos `0x08000605` |

## 2. Estrutura do XEX

| Item | Valor |
|---|---|
| Cabeçalho (texto claro) | 0x0000–0x2FFF (12 KiB) |
| Início da carga PE | 0x3000 |
| Informações de segurança | offset 0xF8; assinatura RSA presente; 191 descritores de página |
| Tamanho da imagem em memória | 12.517.376 bytes (~11,9 MiB) |
| Endereço base / ponto de entrada | `0x82000000` / `0x828050E0` |
| **Criptografia** | **normal (AES-128)** |
| Compressão | básica (4 blocos de dados + preenchimento com zeros) |
| Pilha padrão | 256 KiB; TLS de 64 slots |
| Flags de sistema | `NO_FORCED_REBOOT`, `INSECURE_SOCKETS` |
| Cabeçalhos opcionais | 12 (recursos, formato, entrada, base, importações, checksum, nome PE, bibliotecas estáticas, TLS, pilha, flags, chave LAN) |

### Importações do sistema

| Biblioteca | Versão | Funções importadas |
|---|---|---|
| `xam.xex` | 2.0.21256.0 | 304 |
| `xboxkrnl.exe` | 2.0.21256.0 | 409 |

### Bibliotecas estáticas (XDK 2.0.21256, MSVC 16.0.11886)

`XAV`, `D3D9`, `D3DX9`, `XGRAPHC`, `XAPILIB`, `XBOXKRNL`, **`XUIRUN`, `XUIRNDR`,
`XUIVIDE`, `XUIHTM`** (framework de interface XUI), `XNET`, **`XHTTP`, `XAUTH`**
(HTTP e autenticação), `XAUDIO2`, `XAPOBA`, `XMCORE`, `XACT3`, `X3DAUD`,
`XAVATAR`, `LIBCPMT` e o toolchain `LINK`/`C1`/`C2`.

O que essa lista revela:

- A interface é XUI (o framework de cenas do próprio XDK), com suporte a vídeo
  (`XUIVIDE`, `XAV`) e texto HTML (`XUIHTM`). Os skins do Aurora (`.xzp`) são
  pacotes XUI carregados de fora do executável.
- `XHTTP` mostra que o próprio executável faz requisições HTTP (capas, title
  updates, atualizações).
- `XAVATAR` indica renderização de avatares de perfil.
- Áudio por XAudio2/XACT3.

### Seções de recurso declaradas no cabeçalho

Os nomes e tamanhos ficam em texto claro, mas **o conteúdo está dentro da
imagem criptografada**.

| Nome | Endereço | Tamanho | Interpretação provável (não verificável) |
|---|---|---|---|
| `NovaDir` | `0x82BD0000` | 16 B | ponteiro/diretório do "Nova" (nome da WebUI HTTP do Aurora) |
| `Stage1` | `0x82BD0080` | 7.679 B | código ou carga de estágio inicial |
| `VerData` | `0x82BD1E80` | 48 B | número de versão |
| `LangIcon` | `0x82BD1F00` | 323 B | ícone de idioma |
| `Strings` | `0x82BD2080` | 72.506 B | tabela de textos da interface (idioma padrão) |
| `SysLUA` | `0x82BE3C00` | 531 B | script Lua de sistema (ambiente dos scripts) |
| `DebugTex` | `0x82BE3E80` | 384 B | textura de depuração |
| `Phoenix` | `0x82BE4000` | 9.264 B | dados da equipe (créditos/logo) |

## 3. Extração de recursos: resultado e limitação

**Nenhum recurso interno pôde ser extraído.**

- Entropia da carga PE: 8,000 bits/byte em 188 de 188 janelas de 64 KiB. Isso
  é característico de dados criptografados, não de compressão comum.
- Nenhuma assinatura em texto claro na carga: `MZ`, `XUIZ`, `XDBF`, PNG, DDS,
  RIFF, `<?xml`, URLs. As ~70 "strings" encontradas pelo `strings` são ruído
  aleatório.
- A chave de sessão AES está no próprio cabeçalho, mas cifrada com uma chave
  da plataforma Microsoft. Abrir a imagem exigiria usar essa chave, ou seja,
  **contornar a proteção de código do console**. Isso está fora do escopo
  definido para o projeto e não foi feito.
- Por isso as seções `Strings`, `SysLUA`, `Phoenix` etc. aparecem acima só por
  nome e tamanho.

O que fica acessível de forma legítima, **fora** do XEX:

- O pacote de distribuição do Aurora traz skins, idiomas, scripts Lua e a WebUI
  como arquivos separados. Eles podem ser examinados para entender o
  comportamento, mas continuam sendo propriedade da Phoenix. **Não há licença
  que permita copiá-los** para o Nébula.
- A documentação pública da API Lua do Aurora, no repositório
  [XboxUnity/AuroraScripts](https://github.com/XboxUnity/AuroraScripts). Esse
  repositório **não tem arquivo de licença**: serve como referência de leitura,
  não como código reutilizável.

## 4. Funcionalidades: biblioteca, navegação e capas

Como o binário não pode ser lido, esta seção cruza três fontes: (a) o que o
cabeçalho prova, (b) a documentação e as ferramentas abertas da própria
Phoenix e (c) o comportamento público e conhecido do Aurora. O nível de
evidência de cada item está indicado.

### 4.1 Biblioteca de jogos

| Função | Evidência |
|---|---|
| Varredura de caminhos configurados (HDD, USB, unidades de rede) com profundidade de busca, detectando XEX, GOD (contêiner STFS/SVOD) e XBLA | comportamento conhecido; o FSD2 (GPL) tem a implementação equivalente em `Tools/ContentList` e `Tools/Managers/GameList` |
| Banco SQLite com os itens de conteúdo e os title updates (tabelas `ContentItems`, `TitleUpdates`) | [AuroraDbManager](https://github.com/XboxUnity/AuroraDbManager) (Unlicense) consulta essas tabelas |
| Leitura de metadados do jogo (Title ID, Media ID, nome, ícone) a partir do XEX/SPA/contêiner | importações de `xam`/`xboxkrnl`; o FSD2 tem `Tools/XEX/SPA`, `Xbox360Container` |
| Filtros, ordenações e subtítulos extensíveis por Lua (ex.: ocultar Kinect, ordenar por "último jogado", "ano de lançamento") | API documentada em AuroraScripts (`Filters/`, `Sorts/`, `Subtitles/`) |
| Favoritos, jogos recentes, contagem de vezes jogadas | comportamento conhecido |
| Gerenciamento de title updates baixados do XboxUnity | `XHTTP` no executável; tabela `TitleUpdates` |

### 4.2 Navegação e interface

| Função | Evidência |
|---|---|
| Interface XUI com cenas e skins `.xzp` intercambiáveis | bibliotecas `XUIRUN`/`XUIRNDR` |
| Visualizações da biblioteca (coverflow, lista, grade) com troca por botão | comportamento conhecido |
| Vídeo e texto rico na interface | `XUIVIDE`, `XAV`, `XUIHTM` |
| Seleção de perfil com avatar | `XAVATAR` |
| Pacotes de idioma compilados | [AuroraLanguagePackCompiler](https://github.com/XboxUnity/AuroraLanguagePackCompiler) (Unlicense); recurso `Strings` |
| Scripts utilitários Lua com menus próprios (`Utility`) | AuroraScripts; recurso `SysLUA` |
| WebUI HTTP ("Nova") com API JSON: info do sistema, temperaturas, perfis, conquistas, captura de tela, arquivos | recurso `NovaDir`; a API Nova é a mesma reaproveitada pelo GodStix (ver o relatório do GodStix) |
| Servidor FTP | comportamento conhecido; `INSECURE_SOCKETS` |

### 4.3 Gerenciamento de capas (assets)

O formato dos assets do Aurora está **documentado em código Unlicense** no
[AuroraAssetEditor](https://github.com/XboxUnity/AuroraAssetEditor) (template
`010 Editor Binary Template/AuroraAssetTemplate.bt`):

- Um arquivo `.asset` por jogo, com cabeçalho mágico `RXEA`, versão 1 e
  tamanho dos dados.
- Tipos de imagem: ícone, banner, **boxart (capa)**, slot, fundo e até 20
  screenshots.
- Cada imagem é guardada como textura de GPU do Xbox 360 (cabeçalho de *fetch
  constant* D3D, normalmente DXT e com tiling), pronta para a GPU sem
  conversão.
- Origem das capas: API do XboxUnity (`xboxunity.net/api/Covers/{TitleID}`),
  com capas enviadas pela comunidade; o editor também consulta o catálogo do
  marketplace da Microsoft.

**Observação de direitos:** a arte das capas pertence às editoras dos jogos.
Um dashboard pode baixá-las sob demanda para o console do usuário, como o
Aurora faz, mas o Nébula **não deve incluir capas no pacote** que distribui.

## 5. Código-fonte e licença

| Item | Situação |
|---|---|
| Código-fonte do Aurora | **não publicado**. A Phoenix distribui o Aurora como freeware fechado. Nenhum dos 10 repositórios públicos da organização XboxUnity é o dashboard. |
| Licença do executável | nenhuma licença aberta; a redistribuição de partes do binário ou dos recursos não é autorizada |
| **Freestyle Dash 2 (antecessor, mesma equipe)** | **publicado sob GPL-3.0** em [XboxUnity/freestyledash](https://github.com/XboxUnity/freestyledash). O README diz que partes ficaram de fora "por questões legais". |
| AuroraAssetEditor, AuroraDbManager, AuroraLanguagePackCompiler, AuroraTranslationTool | **Unlicense (domínio público)**. São ferramentas de PC em C#, que documentam os formatos `.asset`, o banco e os pacotes de idioma. |
| AuroraScripts | sem licença: apenas leitura |
| libXexUnpack, Xbox360Toolkit | sem licença detectada; além disso, tratam de desempacotar XEX: **não usar** |

## 6. Conclusão para o Nébula 360

- Nada do `Aurora.xex` pode ser integrado: o conteúdo está inacessível e o
  software é fechado.
- O **conceito** da biblioteca (varredura, banco local, assets por jogo,
  filtros, visualizações, WebUI, FTP) é implementável de forma independente.
  Ideias e funcionalidades não são protegidas por direito autoral, apenas o
  código e a arte.
- Há duas bases legais de apoio: o **FSD2 (GPL-3.0)**, com restrições listadas
  em [`licencas-e-plano.md`](licencas-e-plano.md), e as **ferramentas
  Unlicense**, como especificação dos formatos de dados.
