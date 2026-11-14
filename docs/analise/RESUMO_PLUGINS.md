# Resumo — plugins, módulos e dependências

> 14/11/2026. Detalhes em `PLUGINS_AURORA.md`, `PLUGINS_GODSTIX.md` e
> `DEPENDENCIAS_NEBULA360.md`. "Licença verificada" = conferida em arquivo de
> licença ou cabeçalho do código, nesta análise.

## Componentes de terceiros analisados

| Plugin / componente | Finalidade | Origem | Licença verificada | Necessidade para o Nébula | Implementação própria? | Riscos e limitações |
|---|---|---|---|---|---|---|
| `Aurora.xex` | Dashboard | Phoenix (Aurora 0.7b.2) | Nenhuma (fechado) | Nenhuma (referência funcional) | Sim, é o próprio Nébula | Código cifrado; nada reutilizável |
| `Plugins/FtpDll.xex` | Servidor FTP dentro do Aurora (DLL de título) | Phoenix | Nenhuma | Desnecessário | **Sim** (NebulaFTP no título) | Conflito de porta com outros FTPs |
| `Plugins/Nova.xex` | HUD no Guia durante jogos + NOVA WebUI (DLL de sistema) | Phoenix | Nenhuma | Desnecessário | WebUI: **sim** (no título). HUD em jogo: **não recomendado** | DLL de sistema: conflitos com outros plugins, instabilidade |
| `Plugins/HudScene/*.xur` | Cenas XUI do HUD (temperaturas, arquivos, LiNK, info do jogo) | Phoenix | Nenhuma | Desnecessário | Conceito sim; arquivos não | Dependem do Nova |
| `Plugins/WebRoot/` | Front-end da NOVA WebUI (Angular 4.4.6) | Phoenix + Angular/core-js/zone.js | `app.js`: nenhuma; Angular: MIT | Desnecessário | **Sim** (API e PWA próprias) | Controle remoto amplo (threads, lançamento, arquivos) |
| `Media/Scripts`, `User/Scripts` (Lua) | Filtros, ordenações, subtítulos, utilitários | Phoenix | Nenhuma | Desnecessário | Conceito sim (nível 2 do sistema de plugins) | Repo Browser instala por HTTP **sem verificação de integridade** |
| `Skins/*.xzp`, `Locales/*.xzp`, layouts `.cfljson` | Skin, idiomas, layouts do coverflow | Phoenix, Swizzy, TraJiK | Nenhuma | Desnecessário | Sim (temas e layouts JSON próprios) | — |
| Fontes `xenon*.xtt`, `Arial_16.xpr` | Texto | Microsoft / Monotype | Proprietárias | Desnecessário | Já substituídas (Inter, OFL) | Não redistribuíveis |
| `GodStixStore.xex` | Loja | GodStix | Nenhuma | **Rejeitado** | Loja própria só com homebrew autorizado | Distribui conteúdo comercial sem autorização |
| `plugin/GODStixBg.xex` | Download em segundo plano durante jogos (DLL de sistema) | GodStix | Nenhuma | **Rejeitado** | Não (segundo plano adiado; primeiro plano com retomada já existe) | Reinício ou desligamento ao sair do jogo (documentado); 90 s de espera; depende de desligar `liveblock` |
| `GameInfoScene.xur` modificada | Medidor de download no HUD do Aurora | GodStix (derivado do Aurora) | Nenhuma | **Rejeitado** | — | Sobrescreve arquivo de outro dashboard |
| `xbdm.xex` (no pacote analisado do GodStix) | Monitor de depuração | Microsoft | Proprietária | Desnecessário | Não | Não redistribuível |
| DashLaunch (`launch.xex`) | Carregador, `launch.ini`, plugins de sistema | cOz | **Unlicense** (código publicado em 2026) | **Opcional** (só leitura de opções; nunca escrita) | Não necessário | Plugins de sistema compartilham espaço e memória |
| `ftpdll` do DashLaunch | FTP como plugin de sistema (porta 7564) | cOz + SlimFTPd | Unlicense + **BSD-3-Clause** | Desnecessário | Referência livre para o NebulaFTP | Roda durante jogos (sistema) |
| xsdk | SDK aberto para XEX no kernel de retail | xenosworks | **MIT** (+BSD-3; toolchain GPL-3+) | **Candidato a toolchain** (exige sua autorização) | — | Projeto novo; validação própria necessária |
| homebrew-app-store | Loja de homebrew sobre o xsdk | xenosworks | **MIT** | Referência de desenho | Desenho próprio inspirado | — |
| mbedtls-xenon / curl-xenon | TLS 1.2 / HTTP(S) | xenosworks (ports) | Apache-2.0 ou GPL-2.0+ / licença do curl | Candidatos (loja e downloads) | — | TLS 1.3 ausente; CAs a embarcar |
| xkelib | Declarações de kernel/XAM | cOz | **MIT** | Candidato (se XDK) | — | — |
| XboxTLS | TLS 1.2 com BearSSL | Jakob Rangel | **MIT** (BearSSL MIT) | Candidato (se XDK) | — | Depende de `xtl.h` (XDK) |
| Silverlight360.Https | HTTPS para apps Silverlight360 | Digiex | PolyForm **Noncommercial** + condição | **Rejeitado** | — | Licença não livre |
| australis | Cliente da API do Nova | jrobiche | **GPL-3.0** | Só referência de comportamento | — | — |

## Módulos propostos para o Nébula

| Módulo | Finalidade | Origem | Licença | Necessidade | Situação | Riscos e limitações |
|---|---|---|---|---|---|---|
| NebulaCore | Ciclo de vida, logs, config, módulos, modo seguro | Próprio | Própria (a definir) | **Obrigatório** | Implementado | — |
| NebulaUI | Interface 720p e navegação | Próprio + stb (MIT/PD) + Inter (OFL) | idem | **Obrigatório** | Implementado no PC; falta backend do console | Desempenho no Xenon não medido |
| NebulaLibrary | Biblioteca de jogos | Próprio | idem | **Obrigatório** | Implementado | STFS só com dados sintéticos; lançar GOD/XBLA pendente |
| NebulaCovers | Capas e metadados | Próprio | idem | **Obrigatório** (local) / opcional (online) | Parcial | Direitos de arte: nada é distribuído |
| NebulaPlugins | Registro de módulos | Próprio | idem | **Obrigatório** (interno) | Implementado | Nativos de terceiros rejeitados |
| NebulaStore | Catálogo autorizado | Próprio | idem | Opcional (recomendado) | Parcial | Falta assinatura e TLS |
| NebulaDownloads | Fila de downloads | Próprio | idem | Opcional (exigido pela loja) | Parcial (sem transporte) | Só em primeiro plano |
| NebulaFTP | FTP no título | Próprio | idem | Opcional (desligado) | Não iniciado | Portas, senha |
| NebulaFiles | Gerenciador de arquivos | Próprio | idem | Opcional | Não iniciado | Proteger áreas de sistema |
| NebulaSystem | Temperaturas, memória, armazenamento | Próprio | idem | Opcional | Parcial (espaço livre) | SMC só leitura |
| NebulaMedia | Multimídia | Próprio + decodificadores livres | a verificar | Opcional (tardio) | Não iniciado | Sem XMA por hardware no xsdk; desempenho |
| NebulaEmulators | Lançar emuladores | Próprio | idem | Opcional | Não iniciado | Argumentos de ROM dependem do emulador; sem ROMs/BIOS |
