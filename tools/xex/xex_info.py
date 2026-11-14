#!/usr/bin/env python3
"""xex_info.py - leitor somente-leitura de cabecalhos XEX2 (Xbox 360).

Baseado na documentacao publica do formato XEX2 (free60 / Xenia).
NAO descriptografa nem modifica o arquivo: apenas le os campos em texto
claro do cabecalho, que e o que qualquer ferramenta de inventario faz.

Uso: python3 -I xex_info.py ARQUIVO.xex [--json]
"""
import json
import struct
import sys

HEADER_KEYS = {
    0x000002FF: "RESOURCE_INFO",
    0x000003FF: "FILE_FORMAT_INFO",
    0x000005FF: "DELTA_PATCH_DESCRIPTOR",
    0x00000405: "BASE_REFERENCE",
    0x000080FF: "BOUNDING_PATH",
    0x00008105: "DEVICE_ID",
    0x00010001: "ORIGINAL_BASE_ADDRESS",
    0x00010100: "ENTRY_POINT",
    0x00010201: "IMAGE_BASE_ADDRESS",
    0x000103FF: "IMPORT_LIBRARIES",
    0x00018002: "CHECKSUM_TIMESTAMP",
    0x00018102: "ENABLED_FOR_CALLCAP",
    0x00018200: "ENABLED_FOR_FASTCAP",
    0x000183FF: "ORIGINAL_PE_NAME",
    0x000200FF: "STATIC_LIBRARIES",
    0x00020104: "TLS_INFO",
    0x00020200: "DEFAULT_STACK_SIZE",
    0x00020301: "DEFAULT_FILESYSTEM_CACHE_SIZE",
    0x00020401: "DEFAULT_HEAP_SIZE",
    0x00028002: "PAGE_HEAP_SIZE_AND_FLAGS",
    0x00030000: "SYSTEM_FLAGS",
    0x00040006: "EXECUTION_INFO",
    0x00040201: "TITLE_WORKSPACE_SIZE",
    0x00040310: "GAME_RATINGS",
    0x00040404: "LAN_KEY",
    0x000405FF: "XBOX360_LOGO",
    0x000406FF: "MULTIDISC_MEDIA_IDS",
    0x000407FF: "ALTERNATE_TITLE_IDS",
    0x00040801: "ADDITIONAL_TITLE_MEMORY",
    0x00E10402: "EXPORTS_BY_NAME",
}

MODULE_FLAGS = {
    0x01: "TITLE", 0x02: "EXPORTS_TO_TITLE", 0x04: "SYSTEM_DEBUGGER",
    0x08: "DLL", 0x10: "MODULE_PATCH", 0x20: "PATCH_FULL",
    0x40: "PATCH_DELTA", 0x80: "USER_MODE",
}

SYSTEM_FLAGS = {
    0x00000001: "NO_FORCED_REBOOT", 0x00000002: "FOREGROUND_TASKS",
    0x00000004: "NO_ODD_MAPPING", 0x00000008: "HANDLE_MCE_INPUT",
    0x00000010: "RESTRICTED_HUD_FEATURES", 0x00000020: "HANDLE_GAMEPAD_DISCONNECT",
    0x00000040: "INSECURE_SOCKETS", 0x00000080: "XBOX1_INTEROPERABILITY",
    0x00000100: "DASH_CONTEXT", 0x00000200: "USES_GAME_VOICE_CHANNEL",
    0x00000400: "PAL50_INCOMPATIBLE", 0x00000800: "INSECURE_UTILITY_DRIVE",
    0x00001000: "XAM_HOOKS", 0x00002000: "ACCESS_PII",
    0x00004000: "CROSS_PLATFORM_SYSTEM_LINK", 0x00008000: "MULTIDISC_SWAP",
    0x00010000: "MULTIDISC_INSECURE_MEDIA", 0x00020000: "AP25_MEDIA",
    0x00040000: "NO_CONFIRM_EXIT", 0x00080000: "ALLOW_BACKGROUND_DOWNLOAD",
    0x00100000: "CREATE_PERSISTABLE_RAMDRIVE", 0x00200000: "INHERIT_PERSISTENT_RAMDRIVE",
    0x00400000: "ALLOW_HUD_VIBRATION", 0x00800000: "ACCESS_UTILITY_PARTITIONS",
    0x01000000: "IPTV_INPUT_SUPPORTED", 0x02000000: "PREFER_BIG_BUTTON_INPUT",
    0x04000000: "ALLOW_EXTENDED_SYSTEM_RESERVATION", 0x08000000: "MULTIDISC_CROSS_TITLE",
    0x10000000: "INSTALL_INCOMPATIBLE", 0x20000000: "ALLOW_AVATAR_GET_METADATA_BY_XUID",
    0x40000000: "ALLOW_CONTROLLER_SWAPPING", 0x80000000: "DASH_EXTENSIBILITY_MODULE",
}

ENCRYPTION = {0: "none", 1: "normal (AES)"}
COMPRESSION = {0: "none", 1: "basic (blocos + zeros)", 2: "normal (LZX)", 3: "delta"}
REGION = {0xFFFFFFFF: "all regions"}


def u32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def u16(b, o):
    return struct.unpack_from(">H", b, o)[0]


def flags(value, table):
    return [name for bit, name in table.items() if value & bit]


def cstr(b, o, maxlen=256):
    end = b.find(b"\0", o, o + maxlen)
    return b[o:end if end >= 0 else o + maxlen].decode("latin-1")


def parse(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"XEX2":
        raise SystemExit(f"{path}: nao e XEX2 (magic={data[:4]!r})")
    info = {
        "file": path,
        "file_size": len(data),
        "magic": "XEX2",
        "module_flags": u32(data, 4),
        "module_flags_names": flags(u32(data, 4), MODULE_FLAGS),
        "pe_data_offset": u32(data, 8),
        "security_info_offset": u32(data, 0x10),
        "optional_header_count": u32(data, 0x14),
        "optional_headers": [],
    }
    o = 0x18
    for _ in range(info["optional_header_count"]):
        key, val = u32(data, o), u32(data, o + 4)
        o += 8
        name = HEADER_KEYS.get(key, f"UNKNOWN_{key:08X}")
        h = {"key": f"0x{key:08X}", "name": name}
        size_field = key & 0xFF
        if size_field in (0, 1):
            h["value"] = f"0x{val:08X}"
        else:
            h["offset"] = f"0x{val:08X}"
        info["optional_headers"].append(h)

        if name == "ENTRY_POINT":
            info["entry_point"] = f"0x{val:08X}"
        elif name == "IMAGE_BASE_ADDRESS":
            info["image_base"] = f"0x{val:08X}"
        elif name == "DEFAULT_STACK_SIZE":
            info["default_stack_size"] = val
        elif name == "SYSTEM_FLAGS":
            info["system_flags"] = f"0x{val:08X}"
            info["system_flags_names"] = flags(val, SYSTEM_FLAGS)
        elif name == "ORIGINAL_PE_NAME":
            info["original_pe_name"] = cstr(data, val + 4)
        elif name == "CHECKSUM_TIMESTAMP":
            chk, ts = u32(data, val), u32(data, val + 4)
            import datetime
            info["pe_checksum"] = f"0x{chk:08X}"
            info["pe_timestamp"] = ts
            info["pe_timestamp_utc"] = datetime.datetime.fromtimestamp(
                ts, datetime.timezone.utc).isoformat()
        elif name == "FILE_FORMAT_INFO":
            size = u32(data, val)
            enc, comp = u16(data, val + 4), u16(data, val + 6)
            info["encryption"] = ENCRYPTION.get(enc, enc)
            info["compression"] = COMPRESSION.get(comp, comp)
            info["file_format_info_size"] = size
            if comp == 1:
                blocks = []
                p = val + 8
                while p < val + size:
                    blocks.append((u32(data, p), u32(data, p + 4)))
                    p += 8
                info["basic_compression_blocks"] = len(blocks)
            elif comp == 2:
                info["lzx_window_size"] = f"0x{u32(data, val + 8):X}"
        elif name == "RESOURCE_INFO":
            size = u32(data, val)
            res = []
            p = val + 4
            while p + 16 <= val + size:
                rname = data[p:p + 8].rstrip(b"\0").decode("latin-1")
                res.append({"name": rname,
                            "address": f"0x{u32(data, p + 8):08X}",
                            "size": u32(data, p + 12)})
                p += 16
            info["resources"] = res
        elif name == "STATIC_LIBRARIES":
            size = u32(data, val)
            libs = []
            p = val + 4
            while p + 16 <= val + size:
                lname = data[p:p + 8].rstrip(b"\0").decode("latin-1")
                v = struct.unpack_from(">HHHH", data, p + 8)
                libs.append(f"{lname} {v[0]}.{v[1]}.{v[2]}.{v[3] & 0x7FFF}")
                p += 16
            info["static_libraries"] = libs
        elif name == "IMPORT_LIBRARIES":
            size = u32(data, val)
            strtab_size = u32(data, val + 4)
            count = u32(data, val + 8)
            st = data[val + 12: val + 12 + strtab_size]
            names = [s.decode("latin-1") for s in st.split(b"\0") if s]
            info["import_libraries"] = names
            p = val + 12 + strtab_size
            libs = []
            for _ in range(count):
                lsize = u32(data, p)
                # layout: size, next_digest[0x14], id, version, version_min,
                #         name_index(u16), count(u16)
                ver = u32(data, p + 0x1C)
                vmin = u32(data, p + 0x20)
                nidx = u16(data, p + 0x24)
                nimp = u16(data, p + 0x26)
                def v(x):
                    return f"{x >> 28}.{(x >> 24) & 0xF}.{(x >> 8) & 0xFFFF}.{x & 0xFF}"
                libs.append({"name": names[nidx] if nidx < len(names) else nidx,
                             "version": v(ver), "min_version": v(vmin),
                             "import_count": nimp})
                p += lsize
            info["import_library_details"] = libs
        elif name == "EXECUTION_INFO":
            mid, ver, bver, tid = (u32(data, val), u32(data, val + 4),
                                   u32(data, val + 8), u32(data, val + 12))
            info["execution_info"] = {
                "media_id": f"0x{mid:08X}", "version": f"0x{ver:08X}",
                "base_version": f"0x{bver:08X}", "title_id": f"0x{tid:08X}",
                "platform": data[val + 16], "executable_type": data[val + 17],
                "disc_number": data[val + 18], "disc_count": data[val + 19],
                "savegame_id": f"0x{u32(data, val + 20):08X}",
            }
        elif name == "TLS_INFO":
            info["tls_info"] = [u32(data, val + i * 4) for i in range(4)]

    s = info["security_info_offset"]
    sec = {
        "header_size": u32(data, s),
        "image_size": u32(data, s + 4),
        "rsa_signature_present": any(data[s + 8:s + 8 + 256]),
        "image_flags": f"0x{u32(data, s + 0x10C):08X}",
        "load_address": f"0x{u32(data, s + 0x110):08X}",
        "import_table_count": u32(data, s + 0x128),
        "region": REGION.get(u32(data, s + 0x178), f"0x{u32(data, s + 0x178):08X}"),
        "allowed_media_types": f"0x{u32(data, s + 0x17C):08X}",
        "page_descriptor_count": u32(data, s + 0x180),
    }
    info["security_info"] = sec
    info["pe_payload_bytes"] = len(data) - info["pe_data_offset"]
    return info


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    r = parse(sys.argv[1])
    if "--json" in sys.argv:
        print(json.dumps(r, indent=2, ensure_ascii=False))
    else:
        for k, v in r.items():
            print(f"{k}: {v}")
