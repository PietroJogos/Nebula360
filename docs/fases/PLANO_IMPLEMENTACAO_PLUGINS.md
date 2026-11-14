# Plano de implementação — módulos, plugins e dependências

> 14/11/2026. Complementa `docs/analise/DEPENDENCIAS_NEBULA360.md` e
> `docs/arquitetura/SISTEMA_DE_PLUGINS.md`. Nenhum item deste plano foi
> testado em Xbox 360 real.

## 1. Decisões que dependem de você

| # | Decisão | Opções | Recomendação |
|---|---|---|---|
| D1 | Toolchain do console | (a) **xsdk** (MIT, aberto); (b) XDK (proprietário, exige licença); (c) aguardar | **(a)**, após autorizar o download e a compilação do toolchain e uma validação em console de teste |
| D2 | Licença do Nébula | MIT, GPL-3.0, outra | Escolher antes de publicar binários (hoje o código tem todos os direitos reservados) |
| D3 | Console de testes | modelo da placa (Corona, Jasper, Falcon…), HDD, cabo de rede | Um console dedicado, sem dados importantes, com DashLaunch e um dashboard de emergência (Aurora/XeXMenu) num botão de atalho |
| D4 | Domínio e chaves da loja | quem hospeda o catálogo e guarda a chave privada | Repositório público do catálogo + chave offline |

## 2. Ordem de implementação

Cada etapa só começa quando a anterior passar nos testes do PC e, a partir da
etapa 2, nos testes em console listados.

| Etapa | Entrega | Depende de | Testes de saída |
|---|---|---|---|
| **0 (feito)** | Núcleo, UI no PC, biblioteca, temas, registro de módulos, modo seguro, fila de downloads robusta | — | 75 testes no PC/qemu (seção 4) |
| **1** | `src/platform/xsdk/`: sistema de arquivos, relógio, controle, lançador, apresentação do framebuffer | D1 | No console: tela, controle, leitura de `Hdd1:`/`Usb0:`, gravação em `Hdd1:\Nebula`, "Sair do Nébula" |
| **2** | Lançamento: XEX (já projetado), depois GOD/XBLA e emuladores com argumentos | 1 | Lançar homebrew e voltar; GOD; XBE |
| **3** | NebulaSystem: memória (`MmQueryStatistics`), espaço livre, temperaturas (somente leitura) | 1 | Valores coerentes com o painel do sistema e o DashLaunch |
| **4** | Transporte de rede: sockets + TLS (`mbedtls-xenon`) + CAs/chaves fixadas; detecção de link | 1 | HTTPS para um host de teste; cabo × Wi-Fi; sem link |
| **5** | NebulaStore + Downloads completos: catálogo assinado, anti-rollback, diário de instalação, extração segura | 4, D4 | Instalar/atualizar/remover; queda no meio; disco cheio |
| **6** | NebulaFiles | 1 | Copiar/mover/apagar com confirmação; áreas protegidas |
| **7** | NebulaFTP (desligado por padrão, com senha) e NebulaHTTP opcional | 4 | Conflito de porta; FileZilla; desligar libera a porta |
| **8** | NebulaCovers online (fonte escolhida pelo usuário) | 4 | Sem rede = só capas locais |
| **9** | NebulaMedia (áudio primeiro) | 1 | Reprodução contínua; consumo de CPU |
| **10** | Extensões declarativas (layouts, regras de filtro) | 0 | Validação de esquema |
| Futuro | Scripts em sandbox (nível 2) | 5 (assinatura) | Limites de memória e tempo |
| **Não planejado** | Plugins de sistema / HUD em jogo / downloads em segundo plano | — | Ver `PLUGINS_GODSTIX.md` §2.5 |

## 3. Requisitos de compilação

- **PC (hoje):** CMake ≥ 3.16, GCC ou Clang, opcionalmente SDL2.
  ```bash
  cmake -S . -B build -G Ninja && cmake --build build && ./build/nebula_tests
  ```
- **Verificação big-endian (hoje):** `g++-powerpc-linux-gnu`, `qemu-user`,
  toolchain `cmake/toolchains/powerpc-linux-gnu.cmake`.
- **Console (etapa 1, pendente de D1):** toolchain do xsdk (GCC/binutils
  modificados, newlib), `libxsdk.a`, `tools/elf2xex.py` e as notas de
  terceiros BSD-3 exigidas pelo xsdk em todo título. Alternativa: XDK +
  Visual Studio 2010 (`src/platform/xbox360/README.md`).

## 4. Matriz de testes pedida (seção 9)

Situação:
- **PC ✔**: teste automatizado que passa em x86-64, ASan/UBSan e PowerPC
  big-endian (qemu);
- **Console ✘**: ainda não executado em Xbox 360. **Nenhum teste foi
  aprovado em console.**

| Cenário | Teste automatizado (PC) | Situação PC | Console | O que falta no console |
|---|---|---|---|---|
| Inicialização sem plugins opcionais | `registry_starts_without_optional_modules`, `app_optional_module_disabled_by_config`, `builtin_manifests_are_valid_and_start` | ✔ | ✘ | Etapa 1 |
| Plugin ausente ou incompatível | `registry_handles_missing_and_incompatible_dependencies`, `manifest_parsing_and_policy` | ✔ | ✘ | Etapa 1 |
| Falha de carregamento | `registry_isolates_start_failures`, `app_fails_cleanly_without_fonts` | ✔ | ✘ | Etapa 1 |
| Pouca memória disponível | `registry_low_memory_skips_optional_modules`, `texture_cache_budget_and_failures` | ✔ (orçamento declarado) | ✘ | Ler a memória real (`MmQueryStatistics`) e usá-la como limite; teste com memória ocupada artificialmente |
| Dispositivo USB desconectado | `library_merge_marks_missing_only_in_accessible_roots`, `scanner_detects_all_formats_on_console_tree` (Usb0 ausente) | ✔ | ✘ | Remover o pendrive com o Nébula aberto e durante a varredura |
| Erro de leitura do HDD | `scanner_cancel_and_unreadable_files`, `app_recovers_from_corrupted_files_and_save_failures`, `bootguard_…` (HDD com erro) | ✔ | ✘ | Não há como provocar erro físico com segurança; validar com arquivos sem permissão e partição cheia |
| Falha de conexão | `download_queue_rejects_tampered_data_and_retries` (falhas de abertura), `download_resumes_after_connection_loss` | ✔ | ✘ | Etapa 4 (rede real) |
| Download interrompido | `download_resumes_after_connection_loss`, `download_restarts_when_server_ignores_range`, `download_discards_stale_partial_with_wrong_data`, `download_resumes_after_app_restart`, `download_queue_cancel` | ✔ | ✘ | Etapa 5; desligar o cabo no meio |
| Disco cheio | `download_refuses_when_disk_is_full` | ✔ | ✘ | Etapa 5 |
| Conflitos de portas FTP e HTTP | `registry_detects_port_conflicts` (entre módulos do Nébula) | ✔ | ✘ | Etapa 7: falha de `bind` real (porta ocupada por `ftpdll`/Nova) deve virar aviso |
| Encerramento inesperado | `bootguard_detects_unexpected_shutdown_and_recommends_safe_mode`, `app_enters_safe_mode_after_repeated_unexpected_shutdowns` | ✔ | ✘ | Desligar o console com o Nébula aberto duas vezes e confirmar o modo seguro |
| Retorno seguro ao dashboard | `app_exit_returns_to_dashboard_or_closes_on_pc` | ✔ (lógica e marcação de sessão limpa) | ✘ | `XLaunchNewImage(XLAUNCH_KEYWORD_DEFAULT_APP)` ou `xsdk_launch` no console; RB e atalhos do DashLaunch como saída de emergência |

## 5. Protocolo de testes em console (quando houver)

1. Console dedicado; backup da NAND guardado fora do console; DashLaunch
   com `BUT_X` apontando para um dashboard conhecido.
2. Instalar o Nébula como **XEX comum** (nunca como `Default` na primeira
   rodada).
3. Rodar a lista da seção 4 na ordem, registrando no
   `Hdd1:\Nebula\logs\nebula.log` e anotando modelo da placa, versão do
   kernel e do DashLaunch.
4. Só depois de tudo passar: testar como `Default` (o usuário edita o
   `launch.ini`; o Nébula não edita).
5. Resultados entram em `docs/testes/console/AAAA-MM-DD.md` com evidência
   (log, foto da tela). Sem evidência, o teste não é marcado como aprovado.

## 6. Principais riscos de travamento

| Risco | Onde | Mitigação |
|---|---|---|
| Código em contexto de sistema (DLL de sistema/DPC) | HUD em jogo, downloads em segundo plano | **Não implementar** |
| Esgotar memória do título | Cache de capas, decodificação de mídia, TLS | Orçamentos por módulo; cache LRU limitado; medir no console (etapa 3) |
| Espera bloqueante na rede | Loja, capas online, FTP | Sockets não bloqueantes, timeouts em toda espera, trabalho incremental por quadro |
| Escrita interrompida (queda de energia) | Biblioteca, config, downloads | Gravação atômica, `.part` + verificação, diário de instalação |
| Porta ocupada | FTP/HTTP | Porta configurável; falha de `bind` vira aviso |
| Arquivos malformados | XEX/STFS/JSON/imagens/ZIP | Limites e verificação de limites (já testados); limites de ZIP (etapa 5) |
| Boot em loop como dashboard padrão | `Default` do DashLaunch | Modo seguro após 2 quedas; atalho de emergência; documentação antes de virar `Default` |
| Renderização lenta no Xenon | UI | Medir; se necessário, `IRenderer` acelerado pela GPU |
