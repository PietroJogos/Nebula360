# Camada Xbox 360 (rascunho)

> **Estado: nunca compilada nem executada.** Gerar o executável do console
> (`.xex`) exige o **Xbox 360 XDK** da Microsoft, que não está disponível no
> ambiente de desenvolvimento usado na Fase 2. Este diretório **não** faz
> parte do build CMake.

## O que já está verificado e o que não está

| Peça | Arquivo | Verificação feita |
|---|---|---|
| Núcleo, biblioteca, scanner, temas, loja, interface, `App` | `src/**` (exceto esta pasta) | Compilado em C++03 estrito; 56 testes no PC (x86-64) e em **PowerPC 32-bit big-endian** via `qemu-ppc` |
| Sistema de arquivos Win32 | `src/platform/win32/Win32FileSystem.*` | Só **compilação** contra os cabeçalhos do MinGW (Windows 32-bit) |
| Controle (XInput → `InputState`) | `src/platform/win32/XInputMapping.*` | Só **compilação** contra o `xinput.h` do MinGW |
| Montagem de dispositivos, relógio, lançador, apresentação Direct3D, laço principal | `Xbox360Main.cpp` | **Nenhuma**: rascunho escrito a partir da API pública do Win32/D3D9 |

## Para compilar no console (próxima fase)

1. Windows com Visual Studio 2010 e o **Xbox 360 XDK** instalado sob licença
   da Microsoft.
2. Criar um projeto "Xbox 360 Title" com:
   - todos os `.cpp` de `src/` **exceto** `src/platform/host/`;
   - `src/platform/win32/*.cpp` e `src/platform/xbox360/Xbox360Main.cpp`;
   - `third_party/stb/stb_impl.c`;
   - diretórios de include `src/` e `third_party/stb/`;
   - bibliotecas `xapilib`, `d3d9`, `d3dx9`, `xinput`, `xboxkrnl`.
3. Copiar `assets/` para junto do XEX (o código espera `game:\assets`).
4. Testar em um console RGH/JTAG de desenvolvimento, nesta ordem: abertura
   e desenho da tela; controle; leitura de `Hdd1:`; varredura; gravação em
   `Hdd1:\Nebula`; lançamento de um homebrew XEX.

## Pontos conhecidos a validar

- A declaração das exportações do kernel (`ObCreateSymbolicLink`,
  `RtlInitAnsiString`) pode conflitar com tipos do XDK instalado.
- Desempenho do renderizador por software no Xenon (copiar 3,5 MB por quadro
  para a GPU). Se não atingir 60 quadros/s, a próxima etapa é um
  `IRenderer` em Direct3D (a interface já existe em `src/ui/Renderer.h`).
- Lançar jogos GOD/XBLA e XBE ainda não está implementado (`canLaunch`
  retorna `false` para esses formatos).
- Os offsets do cabeçalho STFS (GOD/XBLA) foram testados só com dados
  sintéticos; precisam ser confirmados com pacotes reais.
