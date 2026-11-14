# Relatório de análise: `GodStix.xex`

> Escopo: análise estática, sem descriptografia, sem execução e sem
> modificação do original. Dados brutos em [`dados/GodStix.header.json`](dados/GodStix.header.json),
> gerados por [`tools/xex/xex_info.py`](../../tools/xex/xex_info.py).

## 1. Identificação

| Campo | Valor |
|---|---|
| Arquivo | `references/xex/original/GodStix.xex` (8.630.272 bytes) |
| SHA-256 | `b85bde3b74c0e5dee48e0f2999c180536fe91731c9e04b76c5b98c24f5d712d9` |
| Formato | XEX2, módulo `TITLE` |
| Nome PE original | **`GodStixStore.exe`** |
| Data de link do PE | **2026-11-09 17:24:14 UTC** (build de 5 dias antes desta análise) |
| Title ID | `0x05AB81F0` (não oficial; Media ID e versão zerados) |
| Autor declarado | "Speedy Games Downloads" (campo `scriptAuthor` do script Lua que acompanha o XEX) |
| Região / mídias | todas as regiões; tipos de mídia permitidos `0x08000605` |

## 2. Estrutura do XEX

| Item | Valor |
|---|---|
| Cabeçalho (texto claro) | 0x0000–0x2FFF |
| Início da carga PE | 0x3000 |
| Informações de segurança | offset 0x100; assinatura RSA presente; 198 descritores de página |
| Tamanho da imagem em memória | 12.976.128 bytes (~12,4 MiB) |
| Endereço base / ponto de entrada | `0x82000000` / `0x82375880` |
| **Criptografia** | **normal (AES-128)** |
| Compressão | básica (4 blocos) |
| TLS | 64 slots, dados TLS de 4.480 bytes em `0x82AC0000` |
| Flags de sistema | `INSECURE_SOCKETS` |
| Cabeçalhos opcionais | 13 (inclui `EXECUTION_INFO`, que o Aurora não tem) |

### Importações e bibliotecas

| Biblioteca | Versão | Funções importadas |
|---|---|---|
| `xam.xex` | 2.0.21256.0 | 96 |
| `xboxkrnl.exe` | 2.0.21256.0 | 320 |

Estáticas (XDK 2.0.21256): `XAPILIB`, `D3D9`, `D3DX9`, `XGRAPHC`, `XBOXKRNL`,
`XNET`, **`XONLINE`**, `XAUDIO2`, `XAPOBA`, `XMCORE`, `LIBCPMT`.

O que essa lista revela:

- **Não usa XUI.** A interface é desenhada diretamente com Direct3D 9 e fontes
  TrueType embutidas, ou seja, um renderizador próprio. Isso é diferente do
  Aurora.
- **Não usa `XHTTP`.** A rede é feita com `XNET`/`XONLINE` (sockets).
  Provavelmente há um cliente HTTP próprio no binário.
- Usa bem menos funções do XAM que o Aurora (96 contra 304), o que é
  compatível com um aplicativo de propósito único (a loja).

### Seções de recurso (nomes em texto claro, conteúdo criptografado)

| Nome | Endereço | Tamanho | Interpretação provável |
|---|---|---|---|
| `ttfarab` | `0x82AC0000` | 843.008 B | fonte TrueType com glifos árabes |
| `ttflat` | `0x82B8DD00` | 414.524 B | fonte TrueType latina |
| `arial` | `0x82BF3080` | 151.564 B | fonte **Arial** (Monotype, proprietária) |
| `font` | `0x82C18100` | 268.300 B | fonte de interface |

As quatro seções são fontes, cerca de 1,6 MiB no total. **Nenhuma pode ser
reaproveitada.** O conteúdo está inacessível e, além disso, a Arial é
licenciada pela Monotype. O Nébula deve usar fontes livres (por exemplo, a
família Noto, sob SIL OFL).

## 3. Extração de recursos: resultado e limitação

**Nenhum recurso interno pôde ser extraído.**

- Entropia da carga: 8,000 bits/byte em 132 de 132 janelas de 64 KiB.
- Nenhuma assinatura em texto claro (`MZ`, PNG, DDS, XML, URL…).
- Abrir a carga exigiria a chave de plataforma da Microsoft (contornar a
  proteção de código). Não foi feito.

## 4. Componentes externos ao XEX (análise do pacote público)

O GodStix também é distribuído como **script Lua do Aurora** (instalado em
`Aurora\User\Scripts\Utility`) mais uma WebUI. Uma cópia desses arquivos está
no repositório de terceiros
[B-Dem/godstix](https://github.com/B-Dem/godstix) ("Analysing GodStix for Xbox
360", commit de 2026-03-17). Esse repositório **não tem licença**. Ele foi
lido apenas para entender o funcionamento. **Nenhum trecho foi copiado para o
Nébula.**

Conteúdo: `main.lua` (~188 KB, versão de script "1.0"), `MenuSystem.lua`,
`JSON.lua`, `GODSend.ini`, um ícone XUI e a WebUI `webuis/Stix` (HTML/JS/CSS,
pontes Node.js para FTP e XBDM, PWA). Também inclui **`xbdm.xex`**, o monitor
de depuração da Microsoft (binário proprietário redistribuído sem
autorização), e **8 XEX de controle de ventoinha** (`fan-speed/*.xex`).

### 4.1 Natureza do catálogo (achado crítico)

O próprio script se descreve assim: *"Browse and download Xbox 360 games from
the GODSend server"*. O catálogo classifica cada arquivo como `game`, `dlc`,
`tu` (title update) ou `translation`, e a instalação vai para
`Content\0000000000000000\<TitleID>\`, o caminho de jogos no formato GOD.

> **O GodStix distribui jogos comerciais de Xbox 360, DLCs e atualizações sem
> licença dos detentores dos direitos.** Fontes públicas o descrevem como uma
> "hShop do Xbox 360" com "mais de 999 jogos". O Nébula **não deve** usar esse
> servidor, essa API ou esse catálogo, nem oferecer uma função equivalente
> para conteúdo comercial.

### 4.2 Loja: categorias, catálogo e contas

Arquitetura cliente-servidor sobre **HTTP sem TLS**, com um único servidor
configurável em `GODSend.ini`:

| Recurso | Endpoint (relativo à API) | Observação |
|---|---|---|
| Teste de conexão | `GET /games?limit=1` | |
| Categorias | `GET /categories` | lista de pares `{value, label}` |
| Catálogo | `GET /games?limit=500` | lista paginada/filtrada |
| Detalhes do jogo | `GET /games/{id}` | título, Title ID, editora, plataforma, descrição, nota média, votos |
| Avaliação | `GET /games/{id}/rate?rating=` | |
| Favoritos | `/favorites`, `/favorites/add`, `/favorites/remove`, `/favorites/check` | por usuário |
| Identificar jogos instalados | `GET /lookup?title_ids=` | cruza Title IDs locais com o catálogo |
| Login | `GET /auth/login?login=&password=` | **senha em texto claro na URL, por HTTP sem criptografia** |
| Nível da conta | `GET /auth/check` | níveis de usuário, limite diário de downloads, dias restantes |
| Modo convidado | `/guest/check`, `/guest/track` | controlado pelo **Console ID** |
| Registro do console | `GET /console/register?console_id=` | |
| Problemas | `GET /report?…` | |

A interface tem: busca, lista alfabética, lista por categoria, tela de
detalhes, "Meus jogos" (varre unidades e pasta `Content` e consulta o
`/lookup`), informações da conta e um seletor de idioma (pt-BR, en…).

### 4.3 Sistema de downloads

| Etapa | Implementação observada |
|---|---|
| Metadados do arquivo | `GET /download/{fileId}/info` devolve tamanho, tipo, `folder_path`, `server_path`, `direct_url` e lista de subarquivos |
| Download | `GET /download/{fileId}?fileIndex=N[&subPath]`, um arquivo de cada vez (jogos GOD têm várias partes `Data0000…`) |
| Gravação | arquivo temporário com nome curto, depois movido para o destino. Nomes são truncados em **42 caracteres** (limite do sistema de arquivos FATX). |
| Robustez | até 3 tentativas por parte (`MaxRetries`); abortar por botão envia `/download/abort` ao servidor |
| Fila | fila de downloads (adicionar, remover, limpar, contar) |
| Destino | escolha da unidade (`Hdd1:`, USB…); caminho `Content\0000000000000000\<TitleID>\` ou `folder_path` do servidor |
| Pós-instalação | verificação da instalação, aviso para desativar o desligamento automático do console, reinício do Aurora para reindexar |
| Controle de uso | limite diário por conta, contador de convidado por Console ID, relatório de download concluído |

APIs do Aurora usadas pelo script: `Http.Get/GetEx`, `FileSystem.*`
(criar/mover/copiar/apagar), `Content.*` (itens instalados), `Sql`,
`Kernel.GetConsoleId`, `Aurora.Restart`.

### 4.4 WebUI "Stix"

É uma variação da WebUI "Nova" do Aurora: mesmos *templates* JSON da API
(`getSystemInfo`, `getTemperatureInfo`, `getProfileInfo`, `getAchievementEntry`,
`getSMCInfo`, `getDashLaunchInfo`, `getScreenCaptureCount`…), PWA (manifest e
service worker), i18n, uma ponte Node.js para FTP e uma ponte para XBDM.

### 4.5 Observações de segurança e privacidade

- **Senha do usuário enviada em texto claro** na query string de uma
  requisição HTTP. Ela fica exposta a qualquer pessoa na rede e nos logs do
  servidor.
- O **Console ID** é enviado ao servidor para identificar o aparelho.
- O script lê **CPU key** e **DVD key** (segredos únicos do console) para
  exibi-los na tela de informações. Na versão analisada, esses valores **não
  são enviados** pela rede: só o Console ID é transmitido.
- Executa XEX de terceiros (ventoinha, XBDM) sem verificação de integridade.

## 5. Código-fonte e licença

| Item | Situação |
|---|---|
| Código-fonte do `GodStix.xex` | **não publicado**; nenhuma licença encontrada |
| Script Lua e WebUI | circulam publicamente, mas **sem licença**. Todos os direitos reservados ao autor. |
| Conteúdo distribuído pela loja | jogos comerciais sem autorização: **incompatível** com o projeto |
| `xbdm.xex` incluído | propriedade da Microsoft: **não redistribuível** |

## 6. Conclusão para o Nébula 360

- **Nada do GodStix pode ser integrado**: nem código, nem recursos, nem
  servidor, nem catálogo.
- Os **conceitos de engenharia** são genéricos e implementáveis do zero:
  catálogo JSON com categorias, tela de detalhes, fila de downloads, download
  em partes com nova tentativa, nomes compatíveis com FATX, cruzamento de
  Title IDs instalados e WebUI com PWA.
- A loja do Nébula deve ser **diferente por desenho**:
  - catálogo só com homebrew de licença livre ou com autorização escrita do
    autor;
  - cada item com licença, URL de origem e hash SHA-256 verificados antes da
    instalação;
  - HTTPS sempre que o console suportar e nenhuma senha em URL;
  - sem coletar identificadores do console.
