# Nébula 360

Central para Xbox 360 (RGH) com biblioteca de jogos, loja de homebrews
gratuitos e autorizados, emuladores, multimídia e ferramentas do sistema.
Desenvolvida como aplicativo próprio, inspirada em ideias do Aurora e do
GodStix, sem reutilizar código ou recursos proprietários.

## Estado atual: fase de análise

| Documento | Conteúdo |
|---|---|
| [docs/analise/Aurora.md](docs/analise/Aurora.md) | relatório do `Aurora.xex` |
| [docs/analise/GodStix.md](docs/analise/GodStix.md) | relatório do `GodStix.xex` |
| [docs/analise/licencas-e-plano.md](docs/analise/licencas-e-plano.md) | matriz de licenças, funcionalidades independentes e plano |
| [references/xex/README.md](references/xex/README.md) | procedência e hashes dos originais (não versionados) |

## Ferramentas

- `tools/xex/xex_info.py`: lê o cabeçalho em texto claro de um XEX2
  (somente leitura, sem descriptografar):

  ```bash
  python3 -I tools/xex/xex_info.py caminho/arquivo.xex --json
  ```
