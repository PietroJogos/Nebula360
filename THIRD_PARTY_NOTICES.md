# Componentes de terceiros

O Nébula 360 é escrito do zero. **Não contém código nem recursos do Aurora
nem do GodStix** (ver `docs/analise/`). Componentes de terceiros incluídos:

| Componente | Onde | Licença | Uso |
|---|---|---|---|
| stb_truetype v1.28, stb_image v2.32, stb_image_write v1.18 (Sean Barrett e colaboradores) | `third_party/stb/` | MIT **ou** domínio público (Unlicense), à escolha — `third_party/stb/LICENSE.txt` | Rasterização de fontes, leitura de PNG/JPEG das capas e gravação de PNG |
| Fonte Inter 4.0 (Regular e SemiBold), The Inter Project Authors | `assets/fonts/` | SIL Open Font License 1.1 — `assets/fonts/OFL.txt` | Texto da interface |

Sem modificações locais. Procedência e versões em
`third_party/stb/VERSION.txt`.

Ferramentas usadas só no desenvolvimento (não distribuídas com o Nébula):
GCC, CMake, Ninja, SDL2 (frontend opcional de PC, zlib), MinGW-w64, QEMU.
