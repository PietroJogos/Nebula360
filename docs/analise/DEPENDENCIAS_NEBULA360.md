# Dependências do Nébula 360 para rodar em um Xbox 360 RGH

> Levantamento de 14/11/2026. Baseado em: análise dos pacotes do Aurora 0.7b.2
> e do GodStix 0.9.2 (`PLUGINS_AURORA.md`, `PLUGINS_GODSTIX.md`), leitura do
> código público do DashLaunch, do xsdk e de bibliotecas de terceiros, e no
> código atual do Nébula. **Nada foi executado em console.** Valores de
> memória marcados como estimativa não foram medidos no Xbox 360.

## 1. Conclusão curta

- **Nenhum plugin externo é necessário.** O Nébula pode ser um **título
  comum (XEX)** com tudo embutido: biblioteca, capas, loja, downloads, FTP e
  HTTP opcionais, arquivos, sistema, mídia, emuladores. Isso é o que o Aurora
  faz com o FTP (`FtpDll.xex` é DLL de título).
- **DLLs de sistema** (estilo Nova, GODStixBg ou plugins do DashLaunch) **não
  são recomendadas**. Só seriam necessárias para funcionar *durante* os jogos
  (HUD no Guia, downloads em segundo plano), e são a principal fonte de
  travamentos e conflitos documentados.
- **O bloqueio principal continua sendo o compilador do console.** O XDK
  (proprietário) segue indisponível. Mas existe uma alternativa aberta que não
  existia no levantamento da Fase 2: o **xsdk** (MIT), com rede, TLS 1.2,
  arquivos, controles e lançamento de XEX declarados como verificados no
  console pelos autores. **Adotá-lo exige sua autorização** (seção 7) e uma
  validação própria.

## 2. Matriz de módulos

Legenda — **Situação**: o que já existe no repositório. **RGH**: compatibilidade
esperada. **Sem plugin externo?**: funciona sem DLL de sistema nem plugin de
terceiros.

| Módulo | Função | Situação no Nébula | Classificação |
|---|---|---|---|
| NebulaCore | inicialização, ciclo de vida, logs, config, módulos, modo seguro | **implementado** (`src/core`, `src/plugins`, `src/app`) | **Obrigatório** |
| NebulaUI | interface e navegação | **implementado** no PC; falta o *backend* gráfico do console | **Obrigatório** |
| NebulaLibrary | detecção e biblioteca | **implementado** | **Obrigatório** |
| NebulaCovers | capas e metadados | **parcial**: capas locais e miniatura STFS; sem fonte online | **Obrigatório** (local); fonte online **opcional** |
| NebulaStore | catálogo autorizado | **parcial**: formato, política e validação | **Opcional** (recomendado) |
| NebulaDownloads | fila de downloads | **parcial**: fila com SHA-256, retomada, espaço livre, limite de velocidade; **sem transporte HTTP/TLS** | **Opcional** (obrigatório para a loja) |
| NebulaFTP | servidor FTP | não iniciado | **Opcional**, desligado por padrão |
| NebulaFiles | gerenciador de arquivos | não iniciado (a base de E/S existe) | **Opcional** (recomendado) |
| NebulaSystem | temperaturas, memória, armazenamento | parcial: espaço livre (`freeSpace`) | **Opcional** (recomendado) |
| NebulaMedia | multimídia | não iniciado | **Opcional** (fase tardia) |
| NebulaEmulators | lançar emuladores | não iniciado (o lançamento de XEX existe no rascunho) | **Opcional** |
| NebulaPlugins | extensões | **registro implementado**: módulos embutidos com manifesto | **Obrigatório** (registro interno); carregamento dinâmico **desnecessário e não recomendado** |

### 2.1 Detalhes por módulo

Memória: as estimativas são cálculos a partir do código, não medições. A
única referência medida no console é do xsdk: um título vazio tem cerca de
**472 MB livres** ([docs/HARDWARE.md, "The heap reserves the title's free
memory"](https://github.com/xenosworks/xsdk)).

#### NebulaCore — obrigatório
- **Dependências:** kernel e XAM (via toolchain); sistema de arquivos;
  relógio.
- **Bibliotecas:** só código próprio (C++03).
- **Permissões/recursos:** escrita em `Hdd1:\Nebula` (config, biblioteca,
  logs, `session.json`).
- **RGH:** sim. É um título comum.
- **Sem plugin externo:** sim.
- **Memória (estimativa):** < 1 MiB, mais o log em memória (512 entradas).
- **Riscos:** HDD ausente ou só leitura. Tratado: avisos na tela, gravação
  atômica e arquivos `.corrompido`.

#### NebulaUI — obrigatório
- **Dependências:** GPU (apresentação a 720p), controles, fontes.
- **Bibliotecas:** stb_truetype/stb_image (MIT/PD, no repositório), fonte
  Inter (OFL-1.1). No console: camada gráfica do xsdk (MIT) **ou** D3D9 do
  XDK. Nenhuma das duas instalada aqui.
- **Permissões/recursos:** GPU e entrada.
- **RGH:** sim, pela API do toolchain.
- **Sem plugin externo:** sim.
- **Memória (estimativa):**
  - framebuffer 1280×720×4 = **3,5 MiB**;
  - fontes carregadas (2 OTF) = **1,2 MiB**, mais o cache de glifos;
  - cache de capas de até 96 × 168×232×4 = **até ~14,6 MiB**.
  - O manifesto declara 24 MiB.
- **Riscos:** desempenho do renderizador por software no Xenon (não medido);
  telas fora de 720p (o GodStix documenta o mesmo problema).

#### NebulaLibrary — obrigatório
- **Dependências:** leitura de `Hdd1:`, `Usb0:` etc.
- **Bibliotecas:** próprias (leitores de cabeçalho XEX2/STFS).
- **Permissões/recursos:** `fs.read`, `fs.write` (biblioteca), `launch`.
- **RGH:** sim.
- **Sem plugin externo:** sim.
- **Memória (estimativa):** ~1 KiB por jogo em memória, mais buffers de
  leitura de cabeçalho de até 1 MiB (XEX) e 38 KiB (STFS).
- **Riscos:**
  - offsets STFS validados só com dados sintéticos;
  - lançamento de GOD/XBLA ainda não implementado;
  - pendrive desconectado já é tratado (não marca jogos como ausentes).

#### NebulaCovers — obrigatório (local) / opcional (online)
- **Dependências:** arquivos de imagem do usuário; opcionalmente HTTP(S).
- **Bibliotecas:** stb_image (PNG/JPEG).
- **Permissões/recursos:** `fs.read`, `fs.write` (cache); `net.client` se
  online.
- **RGH:** sim.
- **Sem plugin externo:** sim.
- **Memória:** compartilha o cache de capas da UI.
- **Riscos:** direitos das capas (o Nébula não distribui arte). Fontes online
  só por escolha do usuário.

#### NebulaStore — opcional
- **Dependências:** NebulaDownloads; catálogo assinado; HTTPS.
- **Bibliotecas:** JSON e SHA-256 próprios. Faltam: verificação de
  assinatura (ECDSA P-256 ou Ed25519) e TLS (`mbedtls-xenon`,
  Apache-2.0/GPL-2.0+).
- **Permissões/recursos:** `net.client`; `fs.write` só em
  `Hdd1:\Nebula\Apps\<id>`.
- **RGH:** depende do TLS no console (o xsdk declara TLS 1.2 verificado).
- **Sem plugin externo:** sim.
- **Memória (estimativa):** catálogo em memória ~2 KiB por item, mais o
  contexto TLS. O Mbed TLS costuma precisar de dezenas de KiB por conexão;
  a referência pública fala em "80 KB" numa proposta para Xbox/360
  ([chupathingyce#581](https://github.com/ChupathingyCE/chupathingyce/pull/581)).
- **Riscos:**
  - certificados: a cadeia de CAs do sistema é antiga (documentado pelo
    [Silverlight360.Https](https://github.com/Digiex/Silverlight360.Https));
    é preciso embarcar um conjunto de CAs ou fixar chaves;
  - curadoria de licenças.

#### NebulaDownloads — opcional (obrigatório para a loja)
- **Dependências:** transporte HTTP(S); espaço livre; relógio.
- **Bibliotecas:** próprias. O transporte virá sobre os sockets do toolchain +
  Mbed TLS.
- **Permissões/recursos:** `net.client`, `fs.write`.
- **RGH:** sim, em primeiro plano. Em segundo plano durante jogos, **não**
  (seção 5.3).
- **Sem plugin externo:** sim, em primeiro plano.
- **Memória:** buffer de 64 KiB, mais o TLS.
- **Riscos:** disco cheio, queda de conexão, servidor sem retomada, parcial
  corrompido. **Já tratados e testados no PC** (`tests/TestDownloads.cpp`).

#### NebulaFTP — opcional, desligado por padrão
- **Dependências:** sockets servidor (TCP `listen`/`accept`).
- **Bibliotecas:** implementação própria. Referência livre: `ftpdll` do
  DashLaunch (Unlicense) + SlimFTPd (BSD-3-Clause).
- **Permissões/recursos:** `net.server`, `fs.read`, `fs.write`; porta
  configurável.
- **RGH:** o xsdk declara servidores TCP. O DashLaunch documenta que o FTP do
  seu configurador e o `ftpdll` (porta 7564) existem.
- **Sem plugin externo:** sim, como serviço **dentro do título** (só enquanto
  o Nébula está aberto).
- **Memória (estimativa):** < 512 KiB.
- **Riscos:**
  - **conflito de porta** com o FTP de outros componentes (`ftpdll` do
    DashLaunch na 7564, FTP do configurador do DashLaunch na 21, FTP do
    Aurora em porta não determinada);
  - acesso sem senha.
  - Mitigação: porta configurável, detecção de falha de `bind` com aviso,
    senha obrigatória e escrita restrita a pastas permitidas.

#### NebulaFiles — opcional
- **Dependências:** `IFileSystem` (já existe).
- **Bibliotecas:** próprias.
- **Permissões/recursos:** `fs.read`, `fs.write`.
- **RGH:** sim.
- **Sem plugin externo:** sim.
- **Memória (estimativa):** < 512 KiB.
- **Riscos:** apagar arquivos do sistema. Mitigação: bloquear escrita em
  `Flash:`, partições de sistema e `$SystemUpdate`; confirmar operações
  destrutivas.

#### NebulaSystem — opcional
- **Dependências:**
  - memória: `MmQueryStatistics` (o xsdk expõe "raw");
  - armazenamento: `freeSpace` (implementado);
  - temperaturas: comando ao SMC pelo kernel, **ou** leitura somente leitura
    dos dados que o DashLaunch já coleta (`tempData` na estrutura exportada,
    segundo `ReadMe/launch_sysdll_exports.c` do DashLaunch).
- **Bibliotecas:** próprias; declarações do kernel do xsdk ou do `xkelib`
  (MIT).
- **Permissões/recursos:** `system.info`.
- **RGH:** provável. Mostrar temperaturas é comum nos dashboards (o Nova
  mostra no Guia).
- **Sem plugin externo:** sim, dentro do título. **Durante jogos**, só com DLL
  de sistema (não recomendado).
- **Memória:** desprezível.
- **Riscos:** chamadas ao SMC mal formadas. **Somente leitura**; nunca
  escrever no SMC (ventoinha) nesta fase. O GodStix inclui XEX de controle de
  ventoinha, que o Nébula não reproduz.

#### NebulaMedia — opcional, fase tardia
- **Dependências:** saída de áudio (o xsdk declara áudio PCM 6 canais 48 kHz
  verificado) e decodificadores **em software**: o xsdk **não** tem
  decodificação XMA em hardware (PARITY.md).
- **Bibliotecas candidatas:** decodificadores de áudio pequenos e
  permissivos; FFmpeg (LGPL) só depois de avaliar licença e desempenho.
  Licenças **a verificar na adoção**.
- **Permissões/recursos:** `fs.read`, `ui`, áudio.
- **RGH:** provável para áudio; vídeo em software a 720p no Xenon é incerto.
- **Sem plugin externo:** sim.
- **Memória:** variável (buffers de decodificação).
- **Riscos:** desempenho; licenças de codecs e patentes.

#### NebulaEmulators — opcional
- **Dependências:** lançar outro XEX com dados (o xsdk declara
  `xsdk_launch_data` até 1020 bytes, verificado).
- **Bibliotecas:** próprias.
- **Permissões/recursos:** `launch`, `fs.read`.
- **RGH:** sim, para emuladores homebrew já existentes.
- **Sem plugin externo:** sim.
- **Memória:** desprezível.
- **Riscos:**
  - **[?]** se cada emulador aceita caminho de ROM por argumento depende do
    emulador; sem isso, o módulo só abre o emulador;
  - licenças dos emuladores variam (algumas não comerciais);
  - **ROMs e BIOS nunca são distribuídos.**

#### NebulaPlugins — obrigatório (registro interno)
- **Dependências:** NebulaCore.
- **Bibliotecas:** próprias (`ModuleRegistry`, `ModuleManifest`, `BootGuard`).
- **Permissões/recursos:** nenhuma; o registro concede permissões aos
  módulos.
- **RGH:** sim (é código comum).
- **Sem plugin externo:** sim.
- **Memória:** desprezível.
- **Riscos:** baixo para módulos embutidos. Carregar DLLs de terceiros: ver
  `docs/arquitetura/SISTEMA_DE_PLUGINS.md`.

### 2.2 Componentes externos avaliados

| Componente | Classificação para o Nébula | Motivo |
|---|---|---|
| DashLaunch (`launch.xex`) | **Opcional, detecção somente leitura** | O Nébula roda sem ele como título. Se presente, as opções podem ser lidas pela API exportada (ordinais 5–7 e 9, em `launch_sysdll_exports.c`) **sem** alterar o `launch.ini` |
| Nova (`Nova.xex`) | Desnecessário | Funções reproduzíveis dentro do título; DLL de sistema fechada |
| `FtpDll.xex` (Aurora) | Desnecessário | Substituído por NebulaFTP próprio |
| `GODStixBg.xex` | **Rejeitado** | Distribui conteúdo não autorizado; instabilidade documentada |
| `xbdm.xex` (incluído no GodStix) | Desnecessário | Ferramenta proprietária da Microsoft |
| xsdk (MIT) | **Candidato a toolchain** | Ver seção 7 |
| xkelib (MIT) | Candidato (só com o XDK) | Declarações de kernel e XAM |
| XboxTLS (MIT, BearSSL MIT) | Candidato (só com o XDK) | Depende de `xtl.h` |
| mbedtls-xenon (Apache-2.0 ou GPL-2.0+), curl-xenon (licença do curl) | Candidatos (com xsdk) | TLS 1.2 / HTTP(S) |
| Silverlight360.Https | **Rejeitado** | Licença PolyForm **Noncommercial** + condição adicional: não é livre |
| australis (GPL-3.0) | Só referência | Cliente da API do Nova; documenta comportamento |

## 3. Permissões e recursos do console (resumo)

| Recurso | Quem precisa | Observação |
|---|---|---|
| Escrita em `Hdd1:\Nebula` | Core, Library, Store, Downloads | Única pasta gravada sem ação explícita do usuário |
| Leitura de `Hdd1:`, `Usb*:`, `Mu*:` | Library, Files, Media | Montagem de dispositivos pela API do toolchain |
| Sockets cliente | Store, Downloads, Covers online | Os sockets do título precisam ser **marcados inseguros pelo próprio título**: segundo o xsdk, só eles alcançam LAN e internet. **Não** depende de `sockpatch` global (a validar no console) |
| Sockets servidor | FTP, HTTP | Portas configuráveis; falha de `bind` vira aviso, nunca travamento |
| Lançar XEX | Library, Emulators | `XLaunchNewImage` (XDK) ou `xsdk_launch` |
| SMC (leitura) | System | Somente leitura |
| Execução durante jogos | **ninguém** nas fases planejadas | Exigiria DLL de sistema |

## 4. DashLaunch e compatibilidade

Fontes: documentação do próprio DashLaunch, publicada pelo autor com o
código em domínio público:
- [cOzInABox/dashLaunch](https://github.com/cOzInABox/dashLaunch), commit
  `bf02a7e` (03/09/2026);
- `ReadMe/info_launch.ini`, `ReadMe/readme.txt`,
  `ReadMe/launch_sysdll_exports.c`.

1. **Como XEX independente.** É o modo recomendado. O Nébula é aberto pelo
   Aurora, FSD, XeXMenu ou um botão de atalho do DashLaunch (`BUT_A`…). Não
   exige nenhuma configuração.
2. **Como dashboard padrão.** O usuário define `Default = Hdd:\Nebula\default.xex`
   no `launch.ini` (o DashLaunch diz: "Default is what loads when you hold no
   buttons"). **O Nébula nunca escreve no `launch.ini`.** A documentação
   mostrará o passo e recomendará, antes, configurar um atalho de emergência
   (`BUT_X = ` caminho do Aurora/XeXMenu).
3. **Plugins do DashLaunch × módulos internos.**
   - Plugins do DashLaunch (`[Plugins] plugin1..plugin5`) são **DLLs de
     sistema** carregadas na inicialização. Ficam ativas entre títulos e
     dentro dos jogos, compartilham o espaço de módulos de sistema e não
     recebem argumentos (issue [#1](https://github.com/cOzInABox/dashLaunch/issues/1)).
   - Os módulos do Nébula são **código dentro do próprio título**: começam e
     terminam com ele.
4. **Compatibilidade com plugins já carregados.** Como título, o Nébula não
   disputa endereços de módulo de sistema. Pode haver concorrência de
   **portas** (FTP) e de **memória do sistema** com plugins já residentes.
   Precedentes documentados de conflito entre plugins: o `ftpdll` mudou o
   endereço de carga para não sobrepor outros, e o Nova só carregou após
   remover um plugin de trainer (relato no RealModScene).
5. **Conflitos com Aurora, Nova e outros.** O Nébula não altera arquivos de
   outros dashboards (ao contrário do GodStix, que substitui uma cena do
   Aurora). Se o Nova estiver residente, a WebUI dele e uma WebUI do Nébula
   podem disputar porta: o NebulaHTTP terá porta configurável, desligado por
   padrão.
6. **Detectar dependências ausentes com segurança.**
   - DashLaunch: obter o módulo `launch.xex` e, se existir, ler opções pelas
     funções exportadas (`GETOPTVALBYNAME`, ordinal 9) **somente leitura**.
     O Nébula nunca usa os ordinais `SETOPTVAL*` (8 e 10).
   - Rede: tentar `XNetGetTitleXnAddr` e o estado do link antes de abrir a
     loja, mostrando o motivo exato.
   - Recursos do próprio Nébula: o `ModuleRegistry` já marca módulos com
     dependência ausente ou incompatível sem derrubar o app.
7. **Voltar ao dashboard anterior se o Nébula falhar.**
   - Menu "Sair do Nébula": implementado; no console chama
     `returnToDashboard()` → `XLaunchNewImage(XLAUNCH_KEYWORD_DEFAULT_APP)`.
     **Não validado no console.**
   - Se o Nébula travar: o DashLaunch documenta que **segurar RB** ao sair
     pelo miniblade volta ao NXE, e que os botões de atalho na inicialização
     desviam do `Default` (depois de o controle sincronizar).
   - `exchandler` (padrão ligado) tenta voltar ao painel em exceções
     recuperáveis e pode gravar o relatório em `Dumpfile`.
   - Dentro do Nébula: o `BootGuard` detecta encerramento inesperado e, após
     2 seguidos, inicia em **modo seguro** (só módulos obrigatórios).

**Proteções de rede:** o Nébula **não** pedirá para desativar `liveblock`
(que bloqueia DNS do LIVE) nem dependerá de `sockpatch`, que dá a **todos**
os títulos o privilégio de socket inseguro. O GodStix pede as duas mudanças;
o Nébula não.

## 5. Loja e gerenciador de downloads

### 5.1 Requisitos × situação

| Requisito | Situação | Onde |
|---|---|---|
| Só homebrew com distribuição autorizada | **Implementado** (política no código) | `src/store/StorePolicy.cpp` |
| HTTPS com verificação de certificado | **Projeto**: Mbed TLS (`mbedtls-xenon`) com conjunto de CAs embarcado ou chaves fixadas. A cadeia nativa do console é antiga | pendente: toolchain do console |
| Verificação SHA-256 | **Implementado e testado** | `DownloadQueue`, `Sha256` |
| Downloads retomáveis | **Implementado e testado** (servidor com e sem *range*, parcial corrompido, retomada após reiniciar) | `tests/TestDownloads.cpp` |
| Controle de espaço livre | **Implementado e testado** (falha sem insistir; folga de 1 MiB) | `IFileSystem::freeSpace` |
| Fila | **Implementado** | `DownloadQueue` |
| Controle de velocidade | **Implementado e testado** (*token bucket*) | `DownloadOptions::rateLimitBytesPerSecond` |
| Perda de conexão | **Implementado e testado** (mantém o `.part`, até N tentativas) | idem |
| Proteção contra arquivos maliciosos | **Parcial**: SHA-256 vindo do catálogo, caminhos FATX validados, instalação só em `Apps\<id>`. **Falta:** assinatura do catálogo e limites de extração de ZIP (entradas, tamanho total, taxa de compressão, *zip-slip*) | `StorePolicy`; pendente |
| Registro de falhas | **Implementado** (log com motivo; `job.error`) | `DownloadQueue` |
| Cancelamento seguro | **Implementado e testado** (apaga o parcial, fecha o transporte) | `TestStore.cpp` |

### 5.2 Desenho recomendado (próximas fases)

1. **Catálogo assinado.** Índice JSON com assinatura (ECDSA P-256 ou
   Ed25519) feita com chave do projeto; a chave pública fica embutida. A
   [homebrew-app-store](https://github.com/xenosworks/homebrew-app-store) (MIT)
   usa ECDSA P-256 e serve como referência de desenho.
2. **Proteção contra rollback.** Recusar catálogo mais antigo que o último
   aceito e manter o último catálogo válido para uso offline.
3. **Diário de instalação.** Gravar `app.json.pending` antes e confirmar
   depois. Uma instalação interrompida é desfeita ou concluída no próximo
   início.
4. **Extração segura.** Limites de entradas, tamanho e taxa; nomes FATX;
   nunca fora de `Apps\<id>`; nada é executado automaticamente após
   instalar.
5. **Transporte.** Sockets do toolchain + Mbed TLS. Timeouts em toda espera:
   o xsdk documenta que o XAM recusa `SO_ERROR`, então uma falha de conexão
   chega só como `EIO`.

### 5.3 Downloads em segundo plano (avaliação)

- **Exige DLL de sistema**, como o `GODStixBg`.
- O próprio GodStix documenta início só 90 s após o jogo, risco de
  **reinício ou desligamento** ao sair do jogo e diferença entre placas.
- O xsdk documenta que plugins de sistema não podem usar `malloc`, stdio nem
  ponto flutuante (podem rodar em DPC) e marca o suporte como não verificado.
- **Decisão: não implementar.** Alternativa: downloads em primeiro plano com
  retomada (já implementada), que continuam de onde pararam quando o usuário
  volta ao Nébula.

## 6. Requisitos de compilação

| Requisito | XDK (proprietário) | xsdk (MIT) |
|---|---|---|
| Disponível neste ambiente | Não | Não instalado. O código-fonte foi clonado **só para leitura**, fora do repositório |
| Licença | Microsoft, só para desenvolvedores licenciados | MIT; partes BSD-3 (libxenon, Xenia); toolchain GCC/binutils (GPL-3+) e newlib |
| Linguagem | C++ do MSVC 2010 (o Nébula já é C++03) | C17 e C++23 (o C++03 do Nébula compila em C++ moderno) |
| Rede/TLS | XNet/XHTTP do XDK + biblioteca TLS (ex.: XboxTLS) | sockets + `mbedtls-xenon` |
| Gráficos | D3D9 Xenon | camada GPU própria |
| Lançar XEX | `XLaunchNewImage` | `xsdk_launch` |
| Carregar DLL de título | Sim (`XexLoadImage`, como o Aurora faz com o `FtpDll`) | Não encontrado no PARITY.md |
| Uso no Nébula | Exige licença que o projeto não tem | **Exige sua autorização** para baixar e compilar o toolchain (uma comunidade cita ~10 GB livres na compilação; não verificado) |

O código atual do Nébula já está isolado da plataforma (`IFileSystem`,
`IClock`, `ILauncher`, `IRenderer`). Portar para o xsdk significa escrever
`src/platform/xsdk/`; o núcleo testado não muda.

## 7. Ferramentas presentes neste ambiente (inspeção)

| Item | Presente | Detalhe |
|---|---|---|
| Compilador C/C++ do PC | Sim | GCC 13.3, Clang |
| Compilador cruzado PowerPC big-endian (Linux) | Sim | `powerpc-linux-gnu-g++` 13.3 + `qemu-ppc` (só para testar ordem de bytes e ponteiros de 32 bits) |
| Compilador Win32 | Sim | MinGW-w64 13 (só compilação da camada Win32/XInput) |
| Compilador para Xbox 360 | **Não** | Nem XDK, nem toolchain do xsdk |
| Bibliotecas gráficas | Só PC | SDL2 2.30 (desenvolvimento); stb no repositório |
| Análise de XEX | Sim (cabeçalho) | `tools/xex/xex_info.py` (próprio, sem descriptografia) |
| Sistema de build | Sim | CMake 3.28, Ninja |
| Rede/armazenamento do console | **Não** | Dependem do toolchain |
| Depuração | Só PC | gdb, ASan/UBSan, qemu. No console seriam o log por rede do xsdk e o `Dumpfile` do DashLaunch. **Não** usar o `xbdm` (proprietário) |
| Extração de pacotes | Sim | `bsdtar` (libarchive, BSD) |

Por instrução, nenhum SDK foi instalado e nenhum executável foi
descriptografado.

## 8. Licenças verificadas nesta análise

| Componente | Licença | Como foi verificada |
|---|---|---|
| stb (no repositório) | MIT ou Unlicense | arquivo no repositório |
| Fonte Inter (no repositório) | OFL-1.1 | arquivo no repositório |
| DashLaunch (código) | Unlicense | `LICENSE` do repositório |
| `ftpdll/ftpd` (SlimFTPd) | BSD-3-Clause | cabeçalho dos arquivos |
| xkelib | MIT | `LICENSE` |
| xsdk | MIT (+ BSD-3 em partes; toolchain GPL-3+) | `LICENSE`, `THIRD_PARTY_NOTICES.md` |
| homebrew-app-store | MIT | `LICENSE` |
| curl-xenon | licença do curl (permissiva) | `COPYING` |
| mbedtls-xenon | Apache-2.0 ou GPL-2.0-or-later | `LICENSE` |
| XboxTLS | MIT; BearSSL MIT | `LICENSE`; cabeçalhos |
| Silverlight360.Https | PolyForm Noncommercial 1.0.0 + condição | `LICENSE.md` |
| australis | GPL-3.0 | `COPYING` |
| Angular 4.4.6 (na WebRoot do Aurora) | MIT | cabeçalho `@license` |
| Aurora, Nova, FtpDll, HudScene, WebRoot (`app.js`), scripts | **Nenhuma** | ausência de licença no pacote |
| GodStixStore, GODStixBg, cena modificada | **Nenhuma** | idem |

Bibliotecas citadas como candidatas e **não** verificadas nesta sessão (Lua,
decodificadores de mídia, bibliotecas de ZIP, conjunto de CAs da Mozilla)
devem ter a licença conferida na versão exata adotada.
