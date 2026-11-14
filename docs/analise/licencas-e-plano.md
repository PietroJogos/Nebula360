# Licenças, funcionalidades independentes e plano de integração

Documento-síntese das análises de [`Aurora.md`](Aurora.md) e
[`GodStix.md`](GodStix.md). **Nenhuma integração foi feita ainda.** Este
documento serve de base para a decisão.

> Aviso: este é um levantamento técnico de licenças, não um parecer jurídico.
> Para distribuição pública, vale confirmar os pontos marcados como "decisão"
> com quem entende de licenciamento.

## 1. Matriz de licenças

| Origem | O que é | Licença | Uso no Nébula |
|---|---|---|---|
| `Aurora.xex` | dashboard compilado (Phoenix) | proprietário, freeware fechado | **não**: conteúdo criptografado e sem licença |
| Skins, idiomas, scripts e WebUI do pacote Aurora | arquivos distribuídos com o Aurora | sem licença aberta | **não** (só como referência de comportamento) |
| [XboxUnity/AuroraScripts](https://github.com/XboxUnity/AuroraScripts) | documentação da API Lua e scripts | sem licença | **apenas leitura** |
| [XboxUnity/AuroraAssetEditor](https://github.com/XboxUnity/AuroraAssetEditor) | editor de `.asset` (C#) | **Unlicense** | **sim**: especificação do formato RXEA; código livre |
| [XboxUnity/AuroraDbManager](https://github.com/XboxUnity/AuroraDbManager) | gerenciador do banco do Aurora (C#) | **Unlicense** | **sim**: referência do esquema (`ContentItems`, `TitleUpdates`) |
| [XboxUnity/AuroraLanguagePackCompiler](https://github.com/XboxUnity/AuroraLanguagePackCompiler), AuroraTranslationTool | ferramentas de idioma (C#) | **Unlicense** | **sim**, se for útil |
| [XboxUnity/freestyledash](https://github.com/XboxUnity/freestyledash) | **código-fonte do Freestyle Dash 2** (C/C++, XDK) | **GPL-3.0** | **sim, com exclusões** (seção 2) e obrigação de copyleft |
| XboxUnity/libXexUnpack, Xbox360Toolkit | desempacotamento de XEX | sem licença detectada | **não** |
| `GodStix.xex` | loja compilada | proprietário, sem licença | **não** |
| Script Lua e WebUI do GodStix ([B-Dem/godstix](https://github.com/B-Dem/godstix)) | cliente da loja | sem licença | **não** (só análise) |
| Catálogo e servidor GodStix | jogos comerciais | sem autorização dos detentores | **não**, em hipótese alguma |
| `xbdm.xex` (incluído no GodStix) | ferramenta Microsoft | proprietário | **não** |

## 2. Freestyle Dash 2 (GPL-3.0): o que pode e o que não pode ser usado

O FSD2 é a única base de **código de dashboard** reutilizável legalmente. A
estrutura dele cobre boa parte do que o Nébula precisa: `Scenes/` (Main,
GameView, FileBrowser, DualPane, ManagePaths, ManageTUs, Media, Screenshots,
SystemInfo, Profile, Achievements, Options, Theme, ChooseSkin) e `Tools/`
(ContentList, GameContent, HTTP, HTTPServer, FTP, FTPClient, SQLite, Settings,
Texture, Managers/{GameList, TitleUpdates, Drives, FileOperation, Skin,
Theme, Screenshot}, XEX/SPA, PKZip).

### 2.1 Arquivos que mesmo assim **não podem** ser integrados

| Arquivo / componente | Motivo |
|---|---|
| `Freestyle/main.cpp`, `Tools/HTTP/Base/HttpClient.cpp`, `Tools/HTTP/Base/MemoryBuffer.h`, `Media/Shaders/Common/DefaultShaders.hlsl` | têm cabeçalho "Copyright (C) Microsoft Corporation". São amostras do XDK, que a Team FSD não tinha como licenciar sob GPL. |
| Usos de `AtgXmlParser.h`, `ATG::Timer`, `ATG::Input` (TUXml, CoverXML, ContentItemNew, ContentManager, KaiVectorXML, ScnAvatar, ScnTeamFSD…) | dependem do *ATG Framework* das amostras do XDK (proprietário, não incluído no repositório). É preciso trocar por um parser XML livre (ex.: tinyxml2, zlib license) e por um timer próprio. |
| `Tools/XEX/XeXtractor.cpp/.h` | contém as **chaves AES de retail e devkit** da Microsoft para descriptografar XEX. Isso é contornar a proteção, e as chaves não pertencem à Team FSD. **Excluir.** |
| `Media/Fonts/Arial_16.xpr` | fonte Arial (Monotype) |
| `Media/nxeart`, `versioner.exe` | arte e binário de origem não comprovada |
| `Tools/ContentList/MarketplaceXML/*`, URLs `catalog.xboxlive.com` | consulta ao catálogo da Microsoft: serviço desativado e termos de uso próprios |
| Clima (`xoap.weather.com`) | API comercial descontinuada |
| XLink Kai (`Tools/XlinkKai`, `Scenes/XLinkKai`) | serviço de terceiros; só com autorização do Team XLink |
| `Skins/Default` (717 arquivos) | precisa de auditoria item a item antes de qualquer uso: pode conter arte de jogos ou da Microsoft. Recomendação: criar um skin próprio. |

### 2.2 Bibliotecas de terceiros no FSD2

| Lib | Licença | Observação |
|---|---|---|
| SQLite 3.6.23.1 (amálgama) | domínio público | é melhor usar uma versão atual do SQLite |
| `Libs/libsmbd` (Samba) | GPL-2.0 "or later" (verificar arquivo a arquivo) | compatível com GPL-3.0 |
| `Libs/libiconv` | LGPL | compatível |
| LodePNG (referenciado) | zlib | compatível |

### 2.3 Consequências de usar código do FSD2

1. **Copyleft:** o Nébula inteiro (o executável que contém código do FSD2)
   passa a ser **GPL-3.0**. O código-fonte completo precisa ser publicado junto
   com cada binário distribuído, e os avisos de copyright da Team FSD/Phoenix
   precisam ser mantidos.
2. **Decisão: toolchain XDK.** O FSD2, o Aurora e o GodStix foram compilados
   com o **Xbox 360 XDK** (versão 2.0.21256), um SDK proprietário da Microsoft
   que só é licenciado a desenvolvedores registrados. Duas questões precisam
   de decisão:
   - De onde vem o XDK usado pelo projeto (licença da Microsoft).
   - Se ligar estaticamente bibliotecas do XDK a código GPL-3.0 é aceitável
     para a distribuição. A exceção de "bibliotecas de sistema" da GPL
     possivelmente não cobre ligação estática. Os autores originais (Team FSD)
     podem fazer isso com o próprio código; terceiros que redistribuem ficam
     numa zona cinzenta.
   - Alternativa 100% livre: [libxenon/Free60](https://free60.org) (GPL-2.0).
     Mas ela roda *bare-metal* via XeLL, sem o kernel e o XAM do console:
     sem perfis, sem XUI, sem os serviços de rede do sistema. É inviável para
     um dashboard que convive com jogos.
3. **Alternativa sem copyleft:** usar o FSD2 apenas como **referência de
   arquitetura**, sem copiar código, e escrever o Nébula do zero com a licença
   que preferirem. É mais trabalho, mas resolve a questão da ligação com o XDK
   sob licença permissiva.

## 3. Funcionalidades implementáveis de forma independente

Funcionalidades, ideias e formatos de interoperabilidade não são protegidos
por direito autoral, apenas o código e a arte. Todos os itens abaixo podem ser
escritos do zero.

### 3.1 Biblioteca de jogos (inspirada no Aurora)

- Varredura configurável de caminhos (HDD, USB, SMB) com profundidade;
  detecção de XEX, GOD/STFS, XBLA e homebrew.
- Leitura de metadados: cabeçalho XEX em texto claro (Title ID, Media ID, nome
  PE, como o `tools/xex/xex_info.py` já faz), SPA/XDBF dos contêineres.
- Banco SQLite local (esquema próprio; o esquema do Aurora documentado pelo
  AuroraDbManager pode guiar uma importação opcional).
- Assets por jogo: ícone, banner, capa, fundo e screenshots. Opção de
  **ler/escrever `.asset` RXEA** (formato documentado em Unlicense), o que
  permite aproveitar as capas que o usuário já tem do Aurora.
- Download de capas **sob demanda** de fonte configurável pelo usuário (ex.:
  API do XboxUnity), sem incluir arte no pacote.
- Visualizações (grade, lista, carrossel), filtros, ordenações, favoritos,
  recentes e contagem de execuções.
- Gerenciamento de title updates (ativar/desativar) dos arquivos que o
  usuário já possui.

### 3.2 Loja de homebrews (conceitos genéricos, desenho próprio)

- Catálogo JSON **assinado**, com categorias (Emuladores, Multimídia,
  Ferramentas, Jogos homebrew, Utilitários), versão, autor, **licença SPDX**,
  URL de origem, tamanho e **SHA-256** de cada arquivo.
- Política de inclusão: só software de licença livre ou com autorização
  escrita do autor, registrada no catálogo.
- Fila de downloads com partes, nova tentativa e retomada; nomes compatíveis
  com FATX (máx. 42 caracteres); verificação do hash antes de instalar.
- Desinstalação e atualização; nenhum dado pessoal ou identificador do console
  enviado; nenhuma senha em URL.

### 3.3 Emuladores

- Lançador de emuladores homebrew já portados para o 360, cada um como XEX
  separado com **sua própria licença** (verificar caso a caso: muitos são
  GPL-2.0-only, que é incompatível com GPL-3.0 dentro do mesmo binário, mas
  não como programas separados).
- Atenção: há emuladores com licença "não comercial" (ex.: Snes9x), que não é
  uma licença livre. Isso precisa constar no catálogo.
- O Nébula não distribui ROMs nem BIOS.

### 3.4 Multimídia

- Visualizador de imagens, player de música e vídeo baseados nos codecs do
  próprio sistema (XMV/WMV via XDK) ou em bibliotecas livres (ex.:
  libvorbis/libopus, BSD; FFmpeg só com avaliação de LGPL e patentes).
- Cliente SMB/UPnP para mídia em rede.

### 3.5 Ferramentas do sistema

- Gerenciador de arquivos com dois painéis, cópia entre unidades, ZIP.
- Servidor FTP e WebUI HTTP **próprios** (API JSON e PWA escritas do zero).
- Informações do sistema: temperaturas, memória, versão do kernel, espaço em
  disco.
- Captura de tela, gerenciamento de perfis e saves.

## 4. Plano proposto (aguardando aprovação)

1. **Decisões do responsável pelo projeto:**
   a) Licença do Nébula: GPL-3.0 (permite usar o FSD2) ou permissiva (escrever
      tudo do zero)?
   b) Toolchain: XDK (qual licença/origem?) ou outra?
   c) Primeiro pilar a construir (sugestão: biblioteca de jogos).
2. **Se GPL-3.0:** importar o FSD2 em `third_party/freestyledash/`, com o
   commit de origem fixado (`df89a42`), removendo os itens da seção 2.1 e
   registrando cada exclusão em `third_party/freestyledash/EXCLUSOES.md`.
3. Especificação dos formatos em `docs/formatos/` (XEX2 em texto claro, RXEA,
   catálogo da loja), escrita por nós a partir das fontes Unlicense e da
   documentação pública.
4. Esqueleto do Nébula com módulos `biblioteca/`, `loja/`, `emuladores/`,
   `multimidia/` e `sistema/`, mais o catálogo inicial da loja só com itens de
   licença verificada.
