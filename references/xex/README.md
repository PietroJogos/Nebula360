# Referências XEX (somente análise)

Executáveis de terceiros fornecidos pelo responsável pelo projeto, usados
**apenas como referência de análise**. Não fazem parte do Nébula 360 e não
são redistribuídos.

| Arquivo        | Tamanho (bytes) | SHA-256                                                            | MD5                              |
|----------------|-----------------|--------------------------------------------------------------------|----------------------------------|
| `Aurora.xex`   | 12.333.056      | `583bcd442d8017d6fcb2645b93cda987f4c0a43a688b652d7364ccaedaeefa9f` | `20638912b5685327460ab37529dc9638` |
| `GodStix.xex`  | 8.630.272       | `b85bde3b74c0e5dee48e0f2999c180536fe91731c9e04b76c5b98c24f5d712d9` | `3db229ddffaf1263fd8057ea02f95900` |

## Regras

- Os originais ficam em `original/`, com permissão somente leitura (`chmod 444`).
  Nenhuma ferramenta do projeto grava nesses arquivos.
- `original/*.xex` está no `.gitignore`. Os dois programas são proprietários
  e sem licença de redistribuição, então **não devem ser versionados nem
  publicados** no repositório. Para reproduzir a análise, coloque cópias com
  os mesmos hashes nesse diretório e confira com:

  ```bash
  sha256sum references/xex/original/*.xex
  ```

- A carga útil dos dois XEX está criptografada (AES, esquema de assinatura da
  Microsoft). O projeto **não descriptografa** esses arquivos. A análise usa
  apenas os campos do cabeçalho, que ficam em texto claro. Veja
  `docs/analise/`.
