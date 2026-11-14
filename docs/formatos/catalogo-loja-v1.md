# Catálogo da loja de homebrews — esquema v1 (arquitetura, Fase 2)

Formato **próprio do Nébula 360** para a futura loja. Nesta fase existem o
leitor, a validação de política, o planejamento de instalação e a fila de
downloads com verificação de integridade. Ainda **não** há servidor, cliente
HTTP nem tela de loja. Código: `src/store/`.

## Política (aplicada por `store::evaluateItemPolicy`)

Um item só entra no catálogo se:

1. tem `id` (a-z, 0-9, `-`, `_`, até 32), `name`, `version`, `author`;
2. a categoria é uma de `emulator`, `media`, `tool`, `utility`,
   `homebrew-game` (não existe categoria para jogos comerciais);
3. declara `license` (identificador SPDX):
   - **licença livre reconhecida** (MIT, BSD, GPL, LGPL, MPL, Apache-2.0,
     Zlib, Unlicense, CC0-1.0, ISC, BSL-1.0, OFL-1.1, WTFPL, 0BSD) com
     `authorization.type` = `license` e referência ao texto da licença; **ou**
   - qualquer outra licença (ex.: "não comercial") **somente** com
     `authorization.type` = `written-permission`, `reference` (documento da
     autorização) e `grantedBy` (quem autorizou);
4. licenças copyleft (GPL/LGPL/AGPL/MPL) exigem `sourceUrl` com o código-fonte;
5. cada arquivo tem `url` http(s), `size` > 0, `sha256` (64 hex) e
   `installPath` relativo, com componentes válidos no FATX (até 42
   caracteres, conjunto restrito, sem `..`, sem `:` e sem `\`).

Itens que violam a política são descartados com o motivo registrado.

## Exemplo (fictício — domínio `.invalid` reservado pela RFC 2606)

```json
{
  "schema": 1,
  "name": "Catálogo de exemplo",
  "updatedAt": "2026-11-14",
  "items": [
    {
      "id": "visualizador-exemplo",
      "name": "Visualizador de Imagens (exemplo)",
      "version": "1.0.0",
      "author": "Autor Exemplo",
      "category": "media",
      "description": "Item fictício usado só para documentar o formato.",
      "license": "MIT",
      "licenseUrl": "https://exemplo.invalid/visualizador/LICENSE",
      "sourceUrl": "https://exemplo.invalid/visualizador/src",
      "authorization": { "type": "license", "reference": "LICENSE" },
      "files": [
        {
          "url": "https://exemplo.invalid/visualizador/default.xex",
          "installPath": "default.xex",
          "size": 524288,
          "sha256": "0000000000000000000000000000000000000000000000000000000000000000"
        }
      ]
    }
  ]
}
```

## Instalação e downloads

- Destino: `<raiz de apps>\<id>\<installPath>` (ex.:
  `Hdd1:\Nebula\Apps\visualizador-exemplo\default.xex`).
- Cada arquivo é baixado para `<destino>.part`, conferido (tamanho e
  SHA-256) e só então renomeado. O download falha se o servidor anunciar ou
  enviar mais bytes que o esperado.
- Até 3 tentativas por arquivo; parciais são apagados a cada falha.
- O transporte é a interface `IDownloadTransport`. Só existe a implementação
  em memória (`MemoryTransport`, para testes).

## Pendências para a fase da loja

- Assinatura do catálogo (ex.: Ed25519), para que só catálogos publicados
  pelo projeto sejam aceitos.
- Cliente HTTP no Xbox 360 (o suporte a TLS do console é limitado: a
  integridade depende do SHA-256 e da assinatura do catálogo, não do
  transporte).
- Curadoria: cada item exige verificação manual da licença ou da
  autorização antes de entrar no catálogo oficial.
