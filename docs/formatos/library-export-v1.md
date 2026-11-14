# Formato de exportação da biblioteca — `nebula360.library` v1

Arquivo gravado pelo Nébula 360 em `<dados>/export/library.v1.json` sempre que
a biblioteca é salva (se `integrations.libraryExport` estiver ativo em
`nebula.json`, o padrão). Serve para que **outros aplicativos** (por exemplo,
um futuro adaptador para o Neutro X) leiam a biblioteca **sem depender do
formato interno** (`library.json`), que pode mudar entre versões.

Gerado por `src/integrations/Integration.cpp` (`buildLibraryExport`).

## Estrutura

```json
{
  "format": "nebula360.library",
  "version": 1,
  "generatedAt": 1794000000,
  "games": [
    {
      "id": "3f2a9c0d1e4b5a67",
      "title": "Lenda do Vale",
      "titleId": "4E450010",
      "category": "xbox360",
      "format": "god",
      "path": "Hdd1:\\Content\\0000000000000000\\4E450010\\00007000\\0123...",
      "cover": "Hdd1:\\Nebula\\cache\\icons\\3f2a9c0d1e4b5a67.png",
      "favorite": true,
      "lastPlayedAt": 1794000000,
      "playCount": 3
    }
  ]
}
```

| Campo | Tipo | Descrição |
|---|---|---|
| `format` | texto | Sempre `"nebula360.library"`. |
| `version` | número | Versão deste formato. Mudanças incompatíveis incrementam o número. |
| `generatedAt` | número | Data da geração (Unix, segundos, UTC). |
| `games[].id` | texto | Identificador estável (16 dígitos hex do SHA-256 do caminho em minúsculas). |
| `games[].title` | texto (UTF-8) | Nome exibido. |
| `games[].titleId` | texto | Title ID em 8 dígitos hex maiúsculos; `"00000000"` se desconhecido. |
| `games[].category` | texto | `xbox360`, `arcade`, `homebrew`, `emulator`, `xbox_classic` ou `demo` (já considerando a escolha do usuário). |
| `games[].format` | texto | `xex` (pasta com `default.xex`), `god`, `xbla` ou `xbe`. |
| `games[].path` | texto | Caminho de lançamento no formato nativo da plataforma. |
| `games[].cover` | texto | Imagem local da capa, ou `""`. |
| `games[].favorite` | booleano | Marcado como favorito. |
| `games[].lastPlayedAt` | número | Última vez iniciado (Unix, segundos); `0` = nunca. |
| `games[].playCount` | número | Quantas vezes foi iniciado pelo Nébula. |

Jogos marcados como ausentes (não encontrados na última varredura) **não**
entram na exportação.

## Regras para leitores

- Ignore campos desconhecidos (versões futuras podem acrescentar campos sem
  mudar `version`).
- Recuse arquivos com `format` diferente ou `version` maior que a suportada.
- O arquivo é substituído de uma vez (gravação em arquivo temporário seguida
  de renomeação), então nunca é lido pela metade.
