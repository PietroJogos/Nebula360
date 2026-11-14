#!/usr/bin/env python3
"""make_demo_data.py - cria uma biblioteca de demonstração 100% fictícia.

Gera, numa pasta de destino:
  jogos/            pastas com default.xex (cabeçalhos XEX2 sintéticos)
  jogos/Content/... contêineres STFS sintéticos (GOD/XBLA) com ícone PNG
  dados/covers/     capas geradas proceduralmente (arte abstrata original)
  dados/nebula.json configuração apontando para jogos/

Nenhum nome, imagem ou arquivo pertence a jogos reais. Uso:
  python3 -I tools/demo/make_demo_data.py DESTINO
"""
import json
import math
import os
import struct
import sys
import zlib


def png_bytes(width, height, pixel):
    """PNG RGBA mínimo; pixel(x, y) -> (r, g, b, a)."""
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            rows.extend(bytes(pixel(x, y)))
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) +
            chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b""))


def hsv(h, s, v):
    i = int(h * 6) % 6
    f = h * 6 - int(h * 6)
    p, q, t = v * (1 - s), v * (1 - f * s), v * (1 - (1 - f) * s)
    r, g, b = [(v, t, p), (q, v, p), (p, v, t), (p, q, v), (t, p, v), (v, p, q)][i]
    return int(r * 255), int(g * 255), int(b * 255)


def cover(width, height, hue, style):
    """Arte abstrata: gradiente + 'planeta' + anéis/estrelas."""
    cx, cy, rad = width * 0.62, height * 0.38, width * 0.30
    def px(x, y):
        t = y / height
        r, g, b = hsv((hue + 0.08 * t) % 1, 0.75, 0.25 + 0.45 * (1 - t))
        d = math.hypot(x - cx, y - cy)
        if d < rad:
            lr, lg, lb = hsv((hue + 0.5) % 1, 0.55, 0.95)
            k = 1 - d / rad * 0.7
            r, g, b = int(lr * k), int(lg * k), int(lb * k)
        elif style == 0 and abs(d - rad * 1.45) < 3 and abs(y - cy) < rad * 0.5:
            r, g, b = 255, 255, 255
        if style == 1 and ((x * 7 + y * 13) % 211 == 0):
            r, g, b = 255, 255, 230
        if style == 2 and (int(x + y * 0.5) // 22) % 2 == 0 and y > height * 0.7:
            r, g, b = min(255, r + 40), min(255, g + 40), min(255, b + 40)
        return (r, g, b, 255)
    return png_bytes(width, height, px)


def xex(title_id=None, pe_name="app.exe"):
    b = bytearray(0x400)
    b[0:4] = b"XEX2"
    struct.pack_into(">IIIII", b, 4, 1, 0x400, 0, 0x100, 0)
    headers = [(0x00010100, 0x82001000), (0x000003FF, 0x300), (0x000183FF, 0x320)]
    struct.pack_into(">IHH", b, 0x300, 8, 1, 1)
    name = pe_name.encode() + b"\0"
    struct.pack_into(">I", b, 0x320, 4 + len(name))
    b[0x324:0x324 + len(name)] = name
    if title_id is not None:
        headers.append((0x00040006, 0x340))
        struct.pack_into(">IIII", b, 0x340, 0x1000 + title_id % 997, 1, 0, title_id)
        b[0x352] = b[0x353] = 1
    struct.pack_into(">I", b, 0x14, len(headers))
    for i, (k, v) in enumerate(headers):
        struct.pack_into(">II", b, 0x18 + i * 8, k, v)
    struct.pack_into(">I", b, 0x100 + 0x178, 0xFFFFFFFF)
    return bytes(b)


def stfs(content_type, title_id, name, icon):
    b = bytearray(0x971A)
    b[0:4] = b"LIVE"
    struct.pack_into(">II", b, 0x344, content_type, 2)
    struct.pack_into(">I", b, 0x360, title_id)
    enc = name.encode("utf-16-be")[:0x7E]
    b[0x411:0x411 + len(enc)] = enc
    b[0x1691:0x1691 + len(enc)] = enc
    struct.pack_into(">II", b, 0x1712, len(icon), len(icon))
    b[0x171A:0x171A + len(icon)] = icon
    b[0x571A:0x571A + len(icon)] = icon
    return bytes(b)


GAMES = [  # (pasta, Title ID fictício ou None = homebrew, estilo da capa ou None)
    ("Asteroides Neon", 0x4E450001, 0), ("Blocos Cadentes", None, 1),
    ("Corrida Estelar", 0x4E450003, 2), ("Dunas de Marte", 0x4E450004, None),
    ("Eco do Abismo", 0x4E450005, 1), ("Fortaleza Pixel", None, None),
    ("Galáxia Perdida", 0x4E450007, 0), ("Horizonte 2099", 0x4E450008, 2),
    ("Ilha dos Ventos", None, None), ("Jardim Quântico", 0x4E45000A, 1),
    ("Órbita Zero", 0x4E45000B, None),
]
EMULATORS = ["Emulador 8-bit Exemplo", "Emulador 16-bit Exemplo"]
APPS = ["Reprodutor de Mídia", "Gerenciador de Arquivos"]
CONTAINERS = [(0x00007000, 0x4E450010, "Lenda do Vale"), (0x000D0000, 0x4E450011, "Torre de Cristal"),
              (0x000D0000, 0x4E450012, "Labirinto Solar")]


def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def main(dest):
    games = os.path.join(dest, "jogos")
    data = os.path.join(dest, "dados")
    for i, (folder, tid, style) in enumerate(GAMES):
        write(os.path.join(games, "Jogos", folder, "default.xex"), xex(tid, "jogo%d.exe" % i))
        if style is not None:
            name = "%08X" % tid if tid else None
            art = cover(300, 420, (i * 0.137) % 1, style)
            if name:
                write(os.path.join(data, "covers", name + ".png"), art)
            else:
                write(os.path.join(games, "Jogos", folder, "cover.png"), art)
    for e in EMULATORS:
        write(os.path.join(games, "Emuladores", e, "default.xex"), xex(None, "emu.exe"))
    for a in APPS:
        write(os.path.join(games, "Apps", a, "default.xex"), xex(None, "app.exe"))
    for i, (ctype, tid, name) in enumerate(CONTAINERS):
        icon = png_bytes(64, 64, lambda x, y, i=i: hsv((0.55 + i * 0.2) % 1, 0.6,
                                                        0.9 if math.hypot(x - 32, y - 32) < 24 else 0.3) + (255,))
        folder = os.path.join(games, "Content", "0000000000000000", "%08X" % tid, "%08X" % ctype)
        write(os.path.join(folder, "%040X" % (tid * 7919)), stfs(ctype, tid, name, icon))
    cfg = {"schema": 1, "theme": "nebula-escuro", "scanPaths": [
        {"path": os.path.join(games, "Jogos"), "category": "auto", "maxDepth": 3, "enabled": True},
        {"path": os.path.join(games, "Emuladores"), "category": "emulator", "maxDepth": 2, "enabled": True},
        {"path": os.path.join(games, "Apps"), "category": "homebrew", "maxDepth": 2, "enabled": True},
        {"path": os.path.join(games, "Content"), "category": "auto", "maxDepth": 4, "enabled": True}]}
    write(os.path.join(data, "nebula.json"), json.dumps(cfg, indent=2, ensure_ascii=False).encode())
    print("dados de demonstração criados em", dest)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    main(os.path.abspath(sys.argv[1]))
