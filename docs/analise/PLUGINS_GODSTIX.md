# Análise de plugins e componentes — GodStix 0.9.2

> Análise estática, somente leitura, feita em 14/11/2026. **Nenhum
> componente foi executado.** Nenhum executável foi descriptografado.
> Nenhum arquivo, servidor ou catálogo do GodStix foi incorporado ao Nébula.

## 0. Pacote analisado

| Item | Valor |
|---|---|
| Arquivo | `godstix.0.9.2 (1).zip` (ZIP sem compressão, 9.007.573 bytes) |
| SHA-256 | `6d4027ee146119b73efa3a3ede5ddbee39c841943af265a7de7aae517fed1565` |
| Cópia local | `references/pacotes/original/` (fora do git, somente leitura) |

| Caminho | Tamanho | Tipo | Função |
|---|---|---|---|
| `godstix/GodStixStore.xex` | 8.630.272 | XEX2, TITLE, AES | Loja (idêntica à analisada na Fase 1, mesmo SHA-256 `b85bde3b…`) |
| `godstix/plugin/GODStixBg.xex` | 196.608 | XEX2, **EXPORTS_TO_TITLE + DLL** (DLL de sistema), AES, LZX | Downloads em segundo plano |
| `godstix/Aurora/Plugins/HudScene/GameInfoScene.xur` | 815 | XUIB | Cena do Aurora modificada |
| `godstix/cover.jpeg` | 183.516 | JPEG | Capa do aplicativo |
| `leia - reader.txt` | 8.394 | texto (pt/en/es) | Manual |
| `Download in background.txt` | 5.362 | texto (pt/en/es/ar) | Instruções do download em segundo plano |
| `HudScene.txt` | 1.573 | texto | Explicação da cena modificada |
| `Tela desconfigurada - Screen display issue.txt` | 1.119 | texto | Problema de resolução (exige 720p ou mais) |

## 1. Natureza do conteúdo distribuído

O manual descreve a loja como um catálogo "de jogos e homebrews" com
instalação automática, e manda escolher o "servidor 3" para baixar jogos
**[arquivo]**. A Fase 1 já mostrou que o catálogo inclui jogos comerciais,
DLCs e *title updates* (`docs/analise/GodStix.md`). **O Nébula não usa nem
reproduz esse catálogo, os servidores ou esse fluxo de distribuição.**

## 2. `GODStixBg.xex` — plugin de download em segundo plano

### 2.1 Cabeçalho (texto claro)

| Campo | Valor |
|---|---|
| Flags de módulo | `EXPORTS_TO_TITLE`, `DLL`: **módulo de sistema** |
| Nome PE original | `GODStixBg.exe` |
| Data do link | 14/09/2026 |
| Endereço base | `0x91E00000` (faixa de módulos de sistema; o Nova usa `0x91A00000`) |
| Tamanho da imagem em memória | 675.840 bytes (660 KiB) |
| Importações | xam 40, xboxkrnl 119 |
| Bibliotecas estáticas | XAPILIB, XBOXKRNL, **XNET**, LIBCPMT |
| Mídias permitidas | `0xFF000000` |
| Carga | cifrada (entropia 7,987); nenhum texto legível |

### 2.2 Funcionamento conceitual

Reconstruído a partir da documentação do próprio pacote **[arquivo]** e do
cabeçalho **[arquivo]**. As partes marcadas são inferência.

1. O usuário **instala o plugin pela página de configuração do GodStix**.
   O manual não diz como a instalação é feita. **[?]** Se é gravando uma
   entrada `pluginN =` no `launch.ini` do DashLaunch não pode ser
   determinado pelos arquivos disponíveis. O script público da versão 1.0
   ([B-Dem/godstix](https://github.com/B-Dem/godstix)) não menciona
   `launch.ini` nem o plugin.
2. Na loja, o usuário escolhe o jogo e o **"servidor 3"**, específico do
   modo em segundo plano.
3. O usuário **inicia um jogo** pela aba "Meus jogos". O download **só
   começa 90 segundos depois que o jogo inicia**.
4. Por isso o usuário precisa escolher perfil e dispositivo de save **antes
   dos 90 s**: depois disso, segundo o manual, não é mais possível.
5. O progresso aparece no Guia, pela `GameInfoScene.xur` modificada (seção 3).
6. O usuário **não deve sair do jogo** antes do fim ou da falha do download:
   "caso contrário pode causar reinício ou desligamento inesperado do
   console" **[arquivo, citação do manual]**.

Inferências técnicas:

- O plugin precisa estar carregado como **módulo de sistema** para
  sobreviver à troca de título (sair da loja e entrar no jogo) **[arquivo:
  flags + base; doc: xsdk GUIDE.md, "A plugin is a system DLL, which the
  kernel keeps loaded across titles"]**.
- A espera de 90 s é coerente com evitar disputa de recursos (memória, E/S,
  rede) com a inicialização do jogo **[inferência]**.
- A instabilidade ao sair do jogo é coerente com o plugin ainda escrevendo
  no disco ou usando rede enquanto o sistema descarrega o título e libera
  recursos **[inferência]**.

### 2.3 Requisitos

| Requisito | Evidência |
|---|---|
| Console desbloqueado com DashLaunch ou outro carregador de DLL de sistema | flags do módulo; manual |
| Rede funcionando para títulos e plugins | manual: ativar `socketpatch` (no DashLaunch, `sockpatch`) e **desativar `liveblock`** |
| Aurora com o HUD do Nova ativo e a cena `GameInfoScene.xur` substituída | `HudScene.txt` |
| Um jogo em execução | manual (o download começa 90 s depois do jogo) |
| Cabo de rede recomendado | manual |

### 2.4 Riscos

| Risco | Evidência | Gravidade |
|---|---|---|
| **Reinício ou desligamento do console** ao sair do jogo durante o download | manual do próprio GodStix | Alta |
| **Corrupção de arquivo** (download parcial) em caso de desligamento | consequência do item acima [inferência] | Alta |
| **Concorrência de memória** com o jogo: módulo de 660 KiB + buffers de rede + gravação em disco, dentro do orçamento de memória do sistema | tamanho da imagem [arquivo]; heap [?] | Média/Alta |
| Comportamento **diferente por modelo de placa** ("o Corona foi muito melhor que o Winchester") | manual | Média |
| **Conflito com outros plugins de sistema**: espaço de endereços e threads compartilhados | precedentes do DashLaunch (`ftpdll` mudou o endereço de carga) e do Nova (relato no RealModScene) | Média |
| **Dependência de arquivo do Aurora**: atualizar ou reinstalar o Aurora desfaz a cena; trocar a cena sem o plugin deixa um elemento não registrado no HUD | `HudScene.txt` + elemento customizado (seção 3) | Média |
| **Redução de proteção de rede** (desligar `liveblock`) | manual; semântica do `liveblock` na documentação do DashLaunch | Alta (contraria a política do Nébula) |
| Perfil e dispositivo de save não escolhíveis depois de 90 s | manual | Baixa/Média |
| Saves e downloads simultâneos no mesmo disco | [inferência] | Média |

### 2.5 Avaliação para o Nébula

- **Downloads em segundo plano durante jogos não serão implementados agora.**
  Exigiriam um módulo de sistema com todas as restrições acima. O próprio
  xsdk marca o suporte a plugins de sistema como "não verificado no console"
  e documenta que esse código **não pode usar `malloc`, stdio nem ponto
  flutuante** (pode rodar em DPC).
- Alternativa estável: **downloads em primeiro plano**, dentro do título do
  Nébula, com retomada após interrupção, verificação SHA-256 e diário de
  instalação. Retomar depois de o jogo terminar dá ao usuário quase o
  mesmo resultado, sem os riscos.
- Um estudo futuro de segundo plano exigiria: console de teste dedicado, sem
  dados importantes; protocolo de testes por modelo de placa; nunca baixar
  diretamente sobre arquivos existentes; e a capacidade de desligar o modo
  por um arquivo de configuração lido na inicialização.

## 3. `GameInfoScene.xur` modificada

| | Aurora original | GodStix |
|---|---|---|
| Tamanho | 696 bytes | 815 bytes |
| Elementos | `fs_GameInfoScene`, `txt_TitleName`, `txt_ExecutionInfo`, `TopBar`, `Info`, `Fader` | os mesmos **+ `GODStixDownloadMeterScene`, `gsbg_DownloadInfo`, `txt_DownloadMeter`** |

O documento `HudScene.txt` confirma: é "uma cópia da versão original do
Aurora" com "uma pequena alteração" para mostrar o progresso do download
**[arquivo]**. `GODStixDownloadMeterScene` é uma classe XUI customizada,
que precisa ser registrada por código em tempo de execução. O único
componente do GodStix ativo durante os jogos é o `GODStixBg`, então o
registro provavelmente é dele **[inferência]**.

Implicações:

1. O GodStix **modifica e redistribui um arquivo do Aurora**, derivado de um
   arquivo sem licença.
2. Cria um acoplamento triplo: Aurora (arquivo) + Nova (HUD) + GODStixBg
   (classe registrada).

## 4. `GodStixStore.xex`

Mesmo binário da Fase 1 (SHA-256 idêntico). Ver `docs/analise/GodStix.md`.
O manual acrescenta:

- histórico de downloads e "baixar tudo" (fila);
- verificação de instalação;
- ordenação por nome, data ou tamanho;
- três idiomas;
- a exigência de resolução de 720p ou mais ("Tela desconfigurada").

## 5. Licenças

| Componente | Licença encontrada |
|---|---|
| `GodStixStore.xex`, `GODStixBg.xex` | Nenhuma (proprietário, fechado) |
| `GameInfoScene.xur` | Nenhuma; derivado de arquivo do Aurora sem licença |
| Manuais, `cover.jpeg` | Nenhuma |
| Catálogo e servidores | Distribuição sem autorização de jogos comerciais (Fase 1). **Incompatível com o Nébula.** |

## 6. Conclusão

Nada do GodStix entra no Nébula. Os conceitos aproveitáveis já estão no
desenho do NebulaDownloads (fila, histórico, verificação, retomada), sem o
modo em segundo plano. A recomendação de desativar o `liveblock` serve como
contraexemplo: o Nébula não exigirá nem fará mudanças de proteção de rede
(ver `DEPENDENCIAS_NEBULA360.md`, seção 4).
