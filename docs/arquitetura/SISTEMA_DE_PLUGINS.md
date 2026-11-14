# Sistema de plugins do Nébula 360

> Estado em 14/11/2026: o **nível 0** (módulos embutidos com manifesto) está
> implementado e testado no PC. Os níveis 1 e 2 estão projetados. O nível 3
> (executáveis nativos de terceiros) foi **avaliado e rejeitado**.

## 1. Objetivo

Acrescentar funcionalidades sem reescrever o aplicativo, mantendo três
garantias:

1. **Estabilidade:** uma extensão que falha não derruba o Nébula e não
   impede o próximo início.
2. **Segurança:** nada de código obtido da internet roda sem verificação de
   integridade e de origem. Executáveis nativos de terceiros não rodam.
3. **Independência:** nenhum plugin do Aurora, do Nova ou do GodStix é
   carregado ou imitado em nível binário.

## 2. Níveis de extensão

| Nível | O que é | Exemplos | Situação |
|---|---|---|---|
| **0 — Módulo embutido** | Código C++ do próprio Nébula, compilado no XEX, com manifesto e ciclo de vida | `nebula.core`, `nebula.ui`, `nebula.library`, `nebula.export`; futuros `nebula.ftp`, `nebula.store`… | **Implementado** (`src/plugins/`, `src/app/BuiltinModules.cpp`) |
| **1 — Extensão declarativa** | Só dados, validados por esquema; nenhum código | temas (já existem), layouts de grade/coverflow, catálogos da loja, regras de filtro/ordenação declarativas, idiomas | Temas e catálogos implementados; layouts e regras declarativas: próximas fases |
| **2 — Script em sandbox** | Script interpretado com permissões declaradas, hash e assinatura | filtros e subtítulos personalizados, utilitários | **Projetado**. O manifesto aceita `"kind": "script"` com `sha256` obrigatório, mas o registro **nunca executa** scripts (não há interpretador) |
| **3 — Executável nativo** | DLL/XEX de terceiros carregada no processo, ou DLL de sistema | o que `FtpDll.xex`, `Nova.xex` e `GODStixBg.xex` são | **Rejeitado.** O manifesto com `"kind": "native"` é recusado com `POLICY_VIOLATION` |

### 2.1 Por que não carregar executáveis nativos

| Pergunta | Evidência | Conclusão |
|---|---|---|
| É tecnicamente possível? | Com o XDK, sim: o Aurora carrega o `FtpDll.xex` (DLL de título). Com o xsdk, não encontrei carregamento dinâmico declarado (PARITY.md). DLLs de sistema: o xsdk tem suporte **não verificado** e restrito (sem `malloc`, stdio nem ponto flutuante) | Depende do toolchain; com xsdk, hoje, não |
| É seguro? | Num console RGH qualquer código não assinado roda, então **assinatura de console não protege nada**. Uma DLL no processo tem acesso total à memória, ao disco e à rede do título | Não há como isolar (não existe *sandbox* de processo para DLLs no título) |
| É estável? | Conflitos documentados entre plugins de sistema (endereço de carga do `ftpdll`; Nova × plugin de trainer); GodStix documenta reinício e desligamento | Uma falha nativa derruba o título (ou o console, no caso de DLL de sistema) |
| É necessário? | Todas as funções planejadas cabem como módulos embutidos (nível 0), dados (nível 1) ou, no futuro, scripts (nível 2) | Não |

Decisão: **sem carregador dinâmico de executáveis.** Se no futuro houver um
caso que só uma DLL resolva, ela entra como **módulo embutido** (compilado no
próprio XEX, revisado e testado), nunca baixado da internet.

## 3. Manifesto (esquema 1)

Mesmo formato para módulos embutidos (texto dentro do código) e, no futuro,
para scripts (arquivo `module.json`). Parser: `parseModuleManifest`
(`src/plugins/ModuleManifest.cpp`).

```json
{
  "schema": 1,
  "id": "nebula.ftp",
  "name": "Servidor FTP",
  "version": "0.1.0",
  "apiVersion": 1,
  "kind": "builtin",
  "required": false,
  "enabledByDefault": false,
  "platforms": ["xbox360", "host"],
  "dependencies": [ { "id": "nebula.core", "minVersion": "0.3.0", "optional": false } ],
  "conflicts": [],
  "permissions": ["net.server", "fs.read", "fs.write"],
  "ports": [21],
  "memoryBudgetKiB": 512,
  "sha256": ""
}
```

| Campo | Regra |
|---|---|
| `id` | 3–48 caracteres: `a-z`, `0-9`, `.`, `-`, `_`; começa com letra; único |
| `version` | `MAIOR.MENOR.CORREÇÃO` |
| `apiVersion` | Precisa ser igual a `ModuleRegistry::kApiVersion` (hoje 1); senão o módulo fica **incompatível** |
| `kind` | `builtin` ou `script`; **`native` é recusado** |
| `required` | Obrigatório: não pode ser desligado; se falhar, `startAll` retorna erro |
| `platforms` | `host` e/ou `xbox360`; vazio = todas |
| `dependencies` | Ausente ou com versão menor que `minVersion` → módulo não inicia (a menos que `optional`) |
| `conflicts` | Não roda junto com os ids listados |
| `permissions` | `fs.read`, `fs.write`, `net.client`, `net.server`, `launch`, `system.info`, `ui`; desconhecida = manifesto inválido |
| `ports` | Portas TCP em que escuta; exige `net.server`; porta repetida entre módulos = conflito |
| `memoryBudgetKiB` | Orçamento declarado, somado contra o limite do registro |
| `sha256` | Obrigatório para `script` |

## 4. Ciclo de vida e regras do registro

`ModuleRegistry::startAll()`:

1. **Avaliação.**
   - Modo seguro ou desligado pelo usuário → `disabled` (só para opcionais).
   - API diferente, plataforma não suportada ou `script` (ainda sem
     interpretador) → `incompatible`.
2. **Ordem.** Ordenação topológica pelas dependências, começando pelos
   obrigatórios. Ciclo → `failed` ("ciclo de dependências").
3. **Para cada módulo, nessa ordem:**
   - dependências obrigatórias rodando e na versão mínima; senão `skipped`
     ou `incompatible`;
   - sem conflito declarado nem porta repetida com módulos já rodando
     (os obrigatórios vêm antes); senão `skipped`;
   - cabe no limite de memória (`setMemoryLimitKiB`); senão `skipped`;
   - `start()`: em erro, o módulo vira `failed`, a instância é destruída e
     quem depende dele vira `skipped`. **O resto continua.**
4. **Resultado.** Erro só se algum **obrigatório** não estiver rodando.

`stopAll()` para na ordem inversa. Todo estado não-`running` tem um motivo
legível (`reason`), registrado no log e mostrado em **Menu › Módulos**.

## 5. Ativação e desativação

- Preferência do usuário: `setEnabled(id, bool)`. Hoje o `nebula.export` é
  controlado por `integrations.libraryExport` em `nebula.json`.
- Próximo passo: uma seção `"modules": {"id": true|false}` em `nebula.json`
  e uma tela para ligar e desligar módulos opcionais (efeito no próximo
  início).
- Obrigatórios ignoram `false`.

## 6. Recuperação de falhas

| Mecanismo | Implementação | Teste |
|---|---|---|
| Falha ao iniciar um módulo opcional | `failed` + dependentes `skipped`; o app segue | `registry_isolates_start_failures` |
| Encerramento inesperado | `BootGuard`: `session.json` com `running` no início, `clean` ao sair ou ao lançar um jogo | `bootguard_detects_unexpected_shutdown_and_recommends_safe_mode` |
| Quedas repetidas | Após **2** encerramentos inesperados seguidos → **modo seguro** (só obrigatórios) + aviso; um encerramento normal volta ao modo normal | `app_enters_safe_mode_after_repeated_unexpected_shutdowns` |
| `session.json` ilegível ou HDD com erro | Não conta como queda; o app inicia sem a proteção e registra o erro | idem |
| Saída controlada | Menu › Sair do Nébula → salva, marca `clean` e volta ao dashboard (no PC, fecha) | `app_exit_returns_to_dashboard_or_closes_on_pc` |

Evolução prevista: contar falhas **por módulo** (qual estava iniciando quando
o processo caiu) e desligar só ele, em vez de todos os opcionais.

## 7. Integridade, confiança e assinatura (níveis 1 e 2)

1. **Origem única:** extensões só vêm do **catálogo assinado do projeto**
   (mesma infraestrutura da loja) ou de uma pasta local que o usuário
   habilitou de propósito ("modo desenvolvedor"), com aviso permanente.
2. **Assinatura:** o índice do catálogo é assinado (ECDSA P-256 ou Ed25519).
   A chave pública fica embutida no XEX. O catálogo contém o `sha256` de
   cada arquivo de extensão.
3. **Integridade local:** antes de carregar, o Nébula recalcula o SHA-256
   do arquivo e compara com o manifesto. Diferença → `incompatible`, com
   motivo.
4. **Permissões mínimas:** o script só acessa as APIs das permissões
   declaradas. `fs.write` fica restrito a `Hdd1:\Nebula\Data\<id>\`.
5. **Recursos limitados (nível 2):** limite de memória por alocador próprio,
   limite de instruções por chamada e tempo máximo por quadro. Estourou →
   o script é interrompido e marcado como `failed`.
6. **Sem efeitos fora do Nébula:** nenhuma extensão pode alterar
   `launch.ini`, NAND, partições de sistema ou arquivos de outros
   dashboards.

## 8. Interface do código

```cpp
class IModule {
 public:
  virtual ~IModule() {}
  virtual Status start(ModuleContext& context) = 0;  // sem exceções; erro = Status
  virtual void stop() = 0;
};

struct ModuleContext {          // serviços oferecidos aos módulos
  PlatformKind platform;
  IFileSystem* fs;
  IClock* clock;
  std::string dataDir;
  IntegrationRegistry* integrations;
};
```

Novos serviços (rede, UI, eventos) entram no `ModuleContext` com aumento do
`apiVersion` quando a mudança for incompatível.

## 9. Testes existentes (PC)

`tests/TestPlugins.cpp` (10 testes) e os testes de integração do `App` em
`tests/TestApp.cpp` (3 testes). Todos passam em x86-64, com ASan/UBSan, e em
PowerPC 32-bit big-endian (qemu). **Nenhum foi executado em Xbox 360.**
