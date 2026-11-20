#!/usr/bin/env python3
# ============================================================
# P.A.S.S. — гвард ISA-базовой линии клиента (раунд #114).
# ------------------------------------------------------------
# Клиент обязан исполняться на целевом CPU (core2-класс: SSE2,
# без AVX/SSE4/POPCNT/LZCNT/BMI/...). GNU-флаги -mno-* это
# гарантируют для gcc/clang, но MSVC intrinsic-и НЕ блокируются
# флагами: __popcnt64 компилируется прямо в POPCNT (F3 0F B8) —
# натурный репрод B-2003: bun_bl.exe → trap → emu → #UD внутри
# pass_client.dll → «internal crash 0xc000001d».
# Этот сканер читает дизасм (dumpbin /DISASM:BYTES — Intel-
# синтаксис, objdump -d — AT&T) и падает с ненулевым кодом,
# если встретил мнемонику вне базового x86-64 (SSE2).
#
# Сканировать надо НАШИ объектники (.o/.obj из
# CMakeFiles/pass_client.dir/src), а не финальный DLL:
# статический CRT (libcmt) содержит ISA-диспетчеризуемые
# memcpy/memset с AVX2-вариантами и __isa_available_init с
# XGETBV — они присутствуют в бинарнике, но на целевом CPU
# никогда не исполняются (CPUID-гвард CRT), т.е. безопасны.
# Наш код таких гвардов не имеет — потому и сканируем его.
#
# Использование:  scan_baseline.py <disasm.txt> [<disasm2.txt> ...]
#   exit 0 — чисто; exit 1 — найдены запрещённые инструкции.
# ============================================================
import re
import sys

# Windows CI: stdout по умолчанию — cp1252 (charmap); русские сообщения
# падали в UnicodeEncodeError уже ПОСЛЕ чистого скана (репрод: run
# 44671909380, шаг Guard Windows). Форсируем UTF-8 для stdout/stderr.
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

# --- точечный denylist (имена мнемоник; AT&T-суффиксы w/l/q/b
#     допускаются автоматически) -------------------------------
DENY_EXACT = [
    # POPCNT / LZCNT / TZCNT
    "popcnt", "lzcnt", "tzcnt",
    # SSE3 (клиент — SSE2-базовая линия)
    "fisttp", "movddup", "movshdup", "movsldup", "lddqu",
    "addsubpd", "addsubps", "haddpd", "haddps", "hsubpd", "hsubps",
    "monitor", "mwait",
    # SSSE3
    "pshufb", "pabsb", "pabsw", "pabsd", "palignr", "pmaddubsw",
    "pmulhrsw", "phaddw", "phaddd", "phaddsw",
    "phsubw", "phsubd", "phsubsw",
    "psignb", "psignw", "psignd",
    # SSE4.1
    "ptest", "pcmpeqq", "packusdw",
    "pextrb", "pextrd", "pextrq",
    "pinsrb", "pinsrd", "pinsrq",
    "pblendvb", "pblendw",
    "pminsb", "pminsd", "pminuw", "pmaxsb", "pmaxsd", "pmaxuw",
    "blendpd", "blendps", "blendvpd", "blendvps",
    "roundpd", "roundps", "roundsd", "roundss",
    "pcmpestri", "pcmpestrm", "pcmpistri", "pcmpistrm",
    "movntdqa", "phminposuw", "mpsadbw", "pmulld",
    # SSE4.2
    "pcmpgtq", "crc32",
    # AES-NI / PCLMULQDQ
    "aesenc", "aesenclast", "aesdec", "aesdeclast",
    "aesimc", "aeskeygenassist",
    "pclmulqdq", "pclmullqlqdq", "pclmulhqlqdq",
    "pclmullqhqdq", "pclmulhqhqdq",
    # RDRAND / RDSEED
    "rdrand", "rdseed",
    # MOVBE / XSAVE-семья / RDTSCP
    "movbe", "xgetbv", "xsetbv",
    "xsave", "xsaveopt", "xsavec", "xsaves",
    "xrstor", "xrstors", "rdtscp",
    # BMI1 / BMI2 / ADX
    "andn", "bextr", "bzhi", "pdep", "pext",
    "sarx", "shlx", "shrx", "mulx", "adcx", "adox",
    # SHA / CLMUL-расширения будущего (шум из ниоткуда не нужен,
    # но класс закрыть)
    "sha1rnds4", "sha1nexte", "sha1msg1", "sha1msg2",
    "sha256rnds2", "sha256msg1", "sha256msg2",
]

# --- AVX/AVX2/AVX-512/FMA/F16C/VBMI: всё VEX/EVES-закодированное
#     начинается с 'v' в имени мнемоники (Intel и AT&T совпадают).
#     Базовых инструкций на 'v' почти нет; легитимные исключения
#     перечислены явно (компилятор их всё равно не генерирует).
DENY_VPREFIX = re.compile(r"\bv[a-z][a-z0-9]*\b")
ALLOW_VPREFIX = {"verr", "verw"}   # 286-protected, базовые

SUFFIX = r"[wlqb]?"
DENY_RE = re.compile(
    r"\b(?:" + "|".join(re.escape(t) for t in DENY_EXACT) + r")" + SUFFIX + r"\b",
    re.IGNORECASE,
)

# Строка дизасма: мнемоника стоит ПОСЛЕ байтов (dumpbin) или после
# таба/адреса (objdump). Перед матчингом вырезаем АННОТАЦИИ СИМВОЛОВ,
# чтобы имена наших функций/переменных не дали ложных срабатываний
# на префиксном правиле AVX ('v'-инициалы): <vtest> у objdump,
# "COMDAT; sym= ..." у dumpbin, комментарии после '#'.
def strip_annotations(line):
    line = re.sub(r"<[^>\n]*>", " ", line)     # objdump: <имя> в метках/переходах
    line = re.sub(r'"[^"\n]*"', " ", line)     # dumpbin: sym= "имя"
    return line.split("#", 1)[0]               # objdump: комментарии

def scan_file(path):
    bad = {}
    try:
        text = open(path, encoding="utf-8", errors="replace").read()
    except OSError as e:
        print(f"  [guard] не удалось открыть {path}: {e}")
        return None
    for ln, line in enumerate(text.splitlines(), 1):
        line = strip_annotations(line)
        for m in DENY_RE.finditer(line):
            tok = m.group(0).lower()
            bad.setdefault(tok, []).append((path, ln, line.strip()[:100]))
        for m in DENY_VPREFIX.finditer(line):
            tok = m.group(0).lower()
            if tok in ALLOW_VPREFIX:
                continue
            bad.setdefault(tok, []).append((path, ln, line.strip()[:100]))
    return bad


def main(argv):
    if len(argv) < 2:
        print("использование: scan_baseline.py <disasm.txt> [...]")
        return 2
    all_bad = {}
    for path in argv[1:]:
        bad = scan_file(path)
        if bad is None:
            return 2
        for tok, hits in bad.items():
            all_bad.setdefault(tok, []).extend(hits)
    if all_bad:
        print("  [guard] FAIL — клиент содержит инструкции вне базового")
        print("          x86-64 (SSE2): на целевом CPU это #UD внутри клиента.")
        for tok in sorted(all_bad):
            print(f"  [guard]   {tok}: {len(all_bad[tok])} вхожд., например:")
            for p, ln, s in all_bad[tok][:3]:
                print(f"            {p}:{ln}: {s}")
        print("  [guard] Убери intrinsic/флаг или расширь описание базовой")
        print("          линии в scripts/scan_baseline.py ОСОЗНАННО.")
        return 1
    print("  [guard] OK — запрещённых мнемоник не найдено (SSE2-базовая линия)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
