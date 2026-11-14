# Análise de plugins e componentes — Aurora 0.7b.2 (pacote de release)

> Análise estática, somente leitura, feita em 14/11/2026. Nenhum componente
> foi executado. Nenhum executável foi descriptografado. Nenhum arquivo do
> pacote foi copiado para o repositório do Nébula.

## 0. Pacote analisado

| Item | Valor |
|---|---|
| Arquivo | `Aurora 0.7b.2 - Release Package.rar` (RAR5, 22.647.806 bytes) |
| SHA-256 | `0c7c765c3a5b938cfc64c71beda276a17d109b88df2a88e3f91eb1e74b7e46a0` |
| Cópia local | `references/pacotes/original/` (fora do git, somente leitura) |
| Extração | `bsdtar` (libarchive) em pasta temporária isolada, depois marcada como somente leitura |
| Entradas | 203 no arquivo; uma entrada sem nome foi ignorada pelo extrator. Todas as entradas com nome foram extraídas. |

Legenda de evidência usada abaixo:

- **[arquivo]** visto diretamente no pacote;
- **[doc]** documentação pública citada;
- **[inferência]** conclusão a partir das evidências, não comprovada;
- **[?]** não determinável com o material disponível.

## 1. Inventário

| Caminho | Tamanho | Tipo (assinatura) | Função |
|---|---|---|---|
| `Aurora.xex` | 12.333.056 | XEX2, TITLE, AES | Dashboard (idêntico ao analisado na Fase 1, mesmo SHA-256) |
| `Plugins/FtpDll.xex` | 200.704 | XEX2, **TITLE + DLL**, AES | Servidor FTP carregado pelo Aurora |
| `Plugins/Nova.xex` | 245.760 | XEX2, **EXPORTS_TO_TITLE + DLL** (DLL de sistema), AES, LZX | Plugin "NOVA": HUD do Guia e WebUI |
| `Plugins/HudScene/*.xur` (13) + `Images/*.png` (33) | ~45 KB + ~120 KB | XUIB (cenas XUI compiladas) + PNG | Telas que o Nova injeta no menu Guia |
| `Plugins/WebRoot/` (53 arquivos) | ~13 MB (11 MB são *source maps*) | HTML/JS/CSS (Angular 4.4.6) | Interface web "NOVA webUI" |
| `Plugins/Log` | — | pasta vazia | Destino de logs dos plugins [inferência] |
| `Media/Scripts/Content/*.lua` (11) | ~27 KB | Lua | Filtros, ordenações e subtítulos embutidos |
| `User/Scripts/**` (10) | ~18 KB | Lua, INI, PNG | Scripts do usuário e o "Aurora Repo Browser" |
| `Media/Layouts/*.cfljson` (12) | 12–41 KB | JSON (`info`, `properties`, `modes`) | Layouts do coverflow |
| `Skins/Default.xzp` | 5.180.269 | XUIZ (pacote XUI) | Skin padrão |
| `Media/Locales/*.xzp` (8) | ~300 KB cada | XUIZ | Idiomas |
| `Media/Fonts/xenonclatin.xtt`, `xenonsclatin.xtt` | 1,2 / 1,4 MB | `xttf` | Fontes de sistema do Xbox 360 (Microsoft) |
| `Media/Fonts/Arial_16.xpr` | 118.796 | XPR2 | Fonte Arial pré-rasterizada (Monotype) |
| `Media/Effects/SimpleShaders.fxobj` | 18.780 | binário de efeito | Shaders compilados |
| `Media/Assets/*.png` (15) | ~3,7 MB | PNG | Arte do próprio Aurora (fundo, banner, capa, screenshots) |
| `nxeart` | 708.608 | STFS `LIVE` | Arte do bloco do Aurora no painel do sistema |
| `live.json` | 3.199 | JSON | Metadados de apresentação ("Compatible with Nova and LiNK") |
| `Data/Thumbnails`, `Data/Logs`, `User/{Icons,Backgrounds,Import,Trainers}` | — | pastas vazias | Estrutura de dados do usuário |

## 2. Cabeçalhos XEX (campos em texto claro)

Lidos com `tools/xex/xex_info.py`, que não descriptografa nada.

| Campo | `Aurora.xex` | `FtpDll.xex` | `Nova.xex` |
|---|---|---|---|
| Flags de módulo | TITLE | TITLE, DLL | EXPORTS_TO_TITLE, DLL |
| Nome PE original | `Aurora.exe` | `FtpDll.dll` | `Nova.dll` |
| Data do link | 10/11/2021 | 10/11/2021 | **23/06/2018** |
| Endereço base | `0x82000000` | `0x85000000` | `0x91A00000` |
| Tamanho da imagem em memória | 12.517.376 (11,9 MiB) | 262.144 (256 KiB) | 675.840 (660 KiB) |
| Importações | xam 304, xboxkrnl 409 | xboxkrnl 120, xam 28 | xam **322**, xboxkrnl 273 |
| Bibliotecas estáticas relevantes | XUI, XHTTP, XNET, XAUDIO2… | **XNET** | **XMCORE** (sem XNET) |
| Recursos | `Strings`, `SysLUA`, `NovaDir`, `Phoenix`… | — | `PlugVer` (48 bytes) |
| Mídias permitidas | `0x08000605` | `0x08000605` | `0xFF000605` |
| Cifra | AES (carga com entropia 8,0) | AES (7,999) | AES (7,990) |

O que os cabeçalhos mostram:

- **`FtpDll.xex` é uma DLL de título.** A base `0x85000000` fica no espaço
  do título, e ela tem a flag TITLE. É carregada **dentro do processo do
  Aurora** e deixa de existir quando o Aurora fecha **[arquivo +
  inferência]**.
- **`Nova.xex` é uma DLL de sistema**, mesmo perfil dos plugins do
  DashLaunch: flag `EXPORTS_TO_TITLE` e base `0x91A00000`, faixa de módulos
  de sistema **[arquivo]**. Um módulo de sistema fica carregado entre
  títulos, inclusive durante os jogos **[doc: xsdk `docs/GUIDE.md`, seção de
  plugins]**.
- **Quem carrega o Nova** **[?]**. Um usuário relatou que "o Aurora carrega
  os plugins da pasta Plugins, sem precisar do DashLaunch" e que o Nova só
  carregou depois de descarregar outro plugin de trainer
  ([RealModScene, 2018](https://www.realmodscene.com/index.php?%2Ftopic%2F8622-aurora-07b-including-nova-by-phoenix%2F=&page=5)).
  É relato de fórum, não documentação oficial.
- Não há nomes de funções importadas em texto claro: a tabela de
  importação do XEX guarda só endereços, e os ordinais estão na carga
  cifrada.

## 3. Componentes

### 3.1 `Aurora.xex` — o dashboard

1. **Função:** biblioteca de jogos (coverflow com layouts JSON), filtros,
   ordenações e subtítulos em Lua, skins XUI, idiomas, scripts
   utilitários, capas (XboxUnity) e title updates.
2. **Dependências:** kernel (`xboxkrnl.exe`) e XAM (`xam.xex`) do sistema,
   bibliotecas estáticas do XDK (XUI, D3D9, XHTTP, XNET, XAUDIO2, XAVATAR),
   arquivos de `Media/`, `Skins/` e `User/`, e opcionalmente os plugins
   abaixo.
3. **Relação:** é o processo principal.
4. **Tipo:** título comum (XEX), iniciado pelo DashLaunch (`Default =` no
   `launch.ini`) ou por outro dashboard.
5. **Reprodução independente:** sim, como funcionalidade (é o que o Nébula
   faz). Nada do binário é reutilizável.
6. **Fonte e licença:** fechado; sem licença de redistribuição. Ferramentas
   de PC da Phoenix (asset editor, gerenciador de banco) são Unlicense (ver
   `docs/analise/Aurora.md`).

### 3.2 `Plugins/FtpDll.xex` — servidor FTP do Aurora

1. **Função:** servidor FTP para acessar os discos do console pela rede
   **[inferência: nome do arquivo e do PE + biblioteca XNET]**.
2. **Dependências:** XNET (rede no contexto de título), kernel e XAM.
3. **Relação:** extensão do processo do Aurora; só existe enquanto o
   Aurora está aberto **[inferência a partir da flag TITLE+DLL]**.
4. **Tipo:** módulo interno de título (DLL carregada pelo Aurora).
5. **Reprodução independente:** sim. Um servidor FTP dentro do próprio
   título do Nébula, sem DLL separada, cobre a mesma função. Referências
   livres verificadas:
   - o `ftpdll` do DashLaunch, Unlicense + BSD-3-Clause do SlimFTPd;
   - os sockets de servidor do xsdk (MIT).
6. **Fonte e licença:** fechado; sem licença.

### 3.3 `Plugins/Nova.xex` — plugin NOVA (HUD + WebUI)

1. **Função:** dois papéis, com evidência:
   - **HUD no menu Guia durante os jogos:** as cenas `Plugins/HudScene/*.xur`
     usam classes do Guia (`XuiGuideNavButton`, `XuiButtonGuide`) e contêm
     aba "Aurora Home" com temperaturas (chassi, CPU, GPU, memória),
     navegador de arquivos, salas de System Link (LiNK), informações de
     jogador, entrada de senha e "GameInfoScene" com nome e Title ID do jogo
     **[arquivo]**;
   - **WebUI:** a pasta `WebRoot` se identifica como "NOVA webUI" no
     `<title>` e nos caminhos internos `Phoenix/NOVA/webui` **[arquivo]**.
     As notas da versão 0.7b citam "New NOVA WebUI" no lugar da WebUI do
     Freestyle Plugin
     ([resumo da comunidade](https://grokipedia.com/page/Aurora_Xbox_360_dashboard)).
2. **Dependências:** XAM (322 importações, coerente com manipulação de UI
   do sistema), kernel, XMCORE. Não tem XNET estática; no console os sockets
   também são exportados pelo XAM **[doc: xsdk `docs/HARDWARE.md`,
   "Sockets reach the LAN…"]**. Usa as cenas em `HudScene` e os arquivos
   de `WebRoot`.
3. **Relação:** complementa o Aurora fora do processo do dashboard,
   continuando ativo dentro dos jogos **[inferência]**.
4. **Tipo:** componente de sistema (DLL de sistema).
5. **Reprodução independente:**
   - **WebUI:** sim, como servidor HTTP **dentro do título** do Nébula (só
     enquanto ele está aberto). Recomendado como opcional.
   - **HUD durante os jogos:** exige uma DLL de sistema que intercepta a UI
     do XAM. É a parte de maior risco (seção 5). **Não recomendado** nas
     primeiras fases do Nébula.
6. **Fonte e licença:** fechado; sem licença. A API da WebUI é usada pelo
   cliente [jrobiche/australis](https://github.com/jrobiche/australis)
   (GPL-3.0), que serve como **documentação de comportamento**, não como
   código a copiar.

### 3.4 `Plugins/HudScene/` — cenas do HUD

| Cena | Elementos encontrados [arquivo] | Função |
|---|---|---|
| `HomeTabSignedIn/SignedOut.xur` (+ variantes `nova*`) | `lbl_ChassisTemp`, `lbl_ProcTemp`, `lbl_GraphicTemp`, `lbl_MemTemp`, `btnFileBrowser`, `btnSystemLink`, `btnDiscInTray` | Aba "Aurora Home" no Guia, com temperaturas |
| `FileBrowser.xur`, `novaFileBrowser.xur` | `List_FileBrowser_Size`, `fs_FBFilesList`, `Path` | Navegador de arquivos no Guia |
| `GameInfoScene.xur` | `txt_TitleName`, `txt_ExecutionInfo` | Informações do jogo em execução |
| `SystemLink.xur`, `CreateRoom.xur`, `PlayerInfo.xur`, `PasswordEntry.xur` | salas, regiões, "Add Friend", "Kick", "Block", senha | System Link pela internet (LiNK) |
| `HudUiSkinCustom.xur`, `novaHudUISkin.xur` | estados visuais de controles | Skin do HUD |

São arquivos de dados (XUI compilado), sem licença: **não reutilizar**. O
GodStix substitui a `GameInfoScene.xur` por uma versão modificada (ver
`PLUGINS_GODSTIX.md`).

### 3.5 `Plugins/WebRoot/` — interface web

1. **Função:** painel web servido pelo Nova. Endpoints vistos no `app.js`
   **[arquivo]**: `systemInfo`, `temperature`, `memory`, `smc`, `threads` e
   `threadState`, `screencapture(s)`, `profileInfo`, `achievementInfo`,
   `titleInfo`, `titleLaunch`, `title/file?path=`, `dashlaunch`,
   `pluginInfo`, `systemLink*`, `updateNotification`, `authenticate`.
   Autenticação por *token* `Bearer` **[arquivo]**.
2. **Dependências:** Angular 4.4.6 (MIT), core-js (MIT), zone.js (MIT); o
   pacote também contém trechos sob Apache-2.0 **[arquivo: cabeçalhos
   `@license` no `vendor.js`/`polyfills.js`]**.
3. **Relação:** front-end do servidor HTTP do Nova.
4. **Tipo:** recurso estático servido por componente de sistema.
5. **Reprodução independente:** sim (API JSON e PWA próprias).
6. **Licença:** as bibliotecas de terceiros são livres, mas o `app.js` da
   Phoenix não tem licença. A porta do servidor **[?]** não aparece nos
   arquivos de forma conclusiva: as URLs `localhost:8000`/`8080` e
   `port:4001` são de desenvolvimento.
7. **Observação de segurança:** a WebUI expõe controle de threads, lançamento
   de títulos e leitura de arquivos pela rede. Uma versão no Nébula deve
   vir desligada por padrão, com senha obrigatória.

### 3.6 Scripts Lua (`Media/Scripts/`, `User/Scripts/`)

1. **Função:**
   - `Media/Scripts/Content`: categorias de filtro (gêneros, diversos,
     nome, usuário), filtros por tipo (Xbox 360, XBLA, clássico, homebrew,
     emuladores, Kinect, indie, apps), 6 ordenações e subtítulos (Title ID,
     Media ID, data, editora, gênero, último uso, caminho virtual).
   - `User/Scripts`: exemplos (esconder backups, Kinect, multidisco;
     ordenar por gênero; código do dispositivo) e o **Aurora Repo Browser**.
2. **Modelo de extensão [arquivo]:** cada script utilitário declara
   `scriptTitle`, `scriptAuthor`, `scriptVersion`, `scriptDescription`,
   `scriptIcon` e **`scriptPermissions`** (ex.: `{"http", "filesystem"}`).
   As APIs usadas são objetos globais (`Script.*`, `Menu.*`, `Http.Get`,
   `FileSystem.*`, `IniFile.*`, `ZipFile.*`, `Content.*`).
3. **Repo Browser [arquivo]:** baixa listas e scripts de
   `http://xboxunity.net/as/*.ini` por **HTTP sem TLS**, extrai ZIPs e
   instala. **Não há verificação de hash, CRC nem assinatura** no código.
   Quem controlar a rede pode trocar o script instalado.
4. **Tipo:** extensões interpretadas dentro do processo do Aurora; a
   linguagem Lua é embutida no `Aurora.xex` (recurso `SysLUA`).
5. **Reprodução independente:** a **ideia** (extensões em script com
   permissões declaradas) é aproveitável e está na arquitetura proposta,
   com integridade obrigatória. Os scripts em si não têm licença no pacote.
   O repositório [XboxUnity/AuroraScripts](https://github.com/XboxUnity/AuroraScripts)
   documenta a API, também sem licença.

### 3.7 Demais recursos

| Recurso | Reutilizável no Nébula? | Motivo |
|---|---|---|
| `Skins/Default.xzp`, `Media/Locales/*.xzp` | Não | Sem licença |
| `Media/Fonts/xenon*.xtt` | Não | Fontes de sistema da Microsoft |
| `Media/Fonts/Arial_16.xpr` | Não | Arial é da Monotype |
| `Media/Layouts/*.cfljson` | Não (arquivos) / Sim (conceito) | Os arquivos são de Phoenix, Swizzy e TraJiK, sem licença. A ideia de layouts parametrizados em JSON pode ser implementada de novo. |
| `Media/Assets/*.png`, `nxeart`, `live.json` | Não | Arte e identidade do Aurora |

## 4. Relação entre os componentes

```
DashLaunch (launch.xex) ──Default=──► Aurora.xex (título)
                                        ├─ carrega FtpDll.xex (DLL de título, vive com o Aurora)
                                        ├─ executa scripts Lua (Media/, User/) com permissões
                                        └─ carrega? Nova.xex (DLL de sistema) [?]
                                                 ├─ injeta cenas HudScene no Guia (durante jogos)
                                                 └─ serve WebRoot (NOVA webUI) por HTTP
```

## 5. Riscos observados

1. **DLL de sistema (Nova):** roda fora do processo do título, dentro dos
   jogos, e compartilha o espaço de módulos de sistema com outros plugins.
   Evidências públicas de conflito:
   - o Nova não carregou até um plugin de trainer ser descarregado (relato
     no RealModScene acima);
   - o próprio DashLaunch registra uma versão do seu FTP que mudou o
     endereço de carga para "não sobrepor mais nada" (`ftpdll/readme.txt`,
     repositório [cOzInABox/dashLaunch](https://github.com/cOzInABox/dashLaunch)).
2. **Scripts sem integridade:** instalação por HTTP sem hash nem assinatura.
3. **WebUI com controle remoto amplo:** lançamento de títulos, threads e
   arquivos.
4. **Substituição de arquivos por terceiros:** o GodStix sobrescreve uma cena
   do Aurora. Uma atualização do Aurora desfaz a mudança, e a mudança
   depende do plugin estar carregado.

## 6. O que o Nébula aproveita (só como referência funcional)

| Ideia | Como entra no Nébula |
|---|---|
| Filtros, ordenações e subtítulos extensíveis | Módulo de biblioteca + futuras extensões em script, com permissões declaradas e hash |
| FTP enquanto o dashboard está aberto | Serviço opcional **dentro do título** (NebulaFTP) |
| WebUI com informações do sistema | Serviço HTTP opcional dentro do título, desligado por padrão e com senha |
| Temperaturas, memória e armazenamento | NebulaSystem dentro do título; durante jogos, **não** nas primeiras fases |
| Layouts de coverflow em JSON | Temas e layouts próprios em JSON (já existe sistema de temas) |
