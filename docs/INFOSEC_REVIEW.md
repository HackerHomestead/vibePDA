# Security Review — vibePDA (Infosec Perspective)

**Date:** 2026-02-16  
**Scope:** Storage layer, input handling, path handling

---

## Findings and Fixes

### 1. read_str / skip_str: Malicious length prefix (HIGH)

**Issue:** Binary files use a 4-byte length prefix before each string. An attacker crafting a .bin file could set `len = 0xFFFFFFFF`. In `read_str`, `(int)len` becomes -1, so `(int)len >= max` is false, and we'd call `fread(buf, 1, 0xFFFFFFFF, f)` — attempting to read 4GB into a small buffer (stack/heap overflow).

**Fix:** Cap `len` to a reasonable maximum (e.g. 16MB) before any read. Reject or truncate when `len > MAX_STRING_LEN`. Use unsigned comparison `len >= (uint32_t)max` for truncation check.

### 2. skip_str: fseek with huge offset (MEDIUM)

**Issue:** `skip_str` does `fseek(f, (long)len, SEEK_CUR)`. With `len = 0xFFFFFFFF`, this seeks 4GB. On 32-bit systems `(long)len` could be -1. Cap len before fseek.

### 3. strcpy in CLI list output (LOW)

**Issue:** `strcpy(content_preview, n->content)` — content_preview is 32 bytes. Storage guarantees null-terminated content, and the `len > 30` branch handles long content. But defense-in-depth: use `strncpy` or `snprintf` to avoid any theoretical overflow if invariants change.

**Fix:** Use `snprintf(content_preview, sizeof(content_preview), "%s", n->content)` for the short case.

### 4. Path validation (LOW)

**Issue:** `--data-dir` accepts user input. If passed `../../../etc`, we could write outside intended directory. For a local CLI tool this is low risk (user controls their process), but validating no `..` segments is good practice.

**Fix:** Reject data_dir containing `..` when it could escape the intended base.

### 5. Integer overflow in read_str fseek (LOW)

**Issue:** `fseek(f, (long)(len - (max - 1)), SEEK_CUR)` — if len is huge and max is 4096, `len - (max-1)` could overflow uint32_t. Cap len first.

---

## Checklist

| Item | Status |
|------|--------|
| read_str len cap | Fixed |
| skip_str len cap | Fixed |
| strcpy → snprintf | Fixed |
| Path traversal check | Optional (deferred) |
