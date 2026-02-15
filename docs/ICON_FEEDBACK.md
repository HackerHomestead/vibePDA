# Icon Design Feedback — Product & Marketing Review

**Date:** 2026-02-15  
**Reviewer perspective:** Product Manager with marketing background  
**Asset:** `assets/icon.svg`, `assets/icon.png`

---

## Executive Summary

The icon effectively communicates **terminal** and **PDA** through a clean, dark-mode aesthetic. It aligns with the target audience (developers, terminal users) and the 1980s-inspired brand. Recommended refinements focus on scalability, light-background visibility, and brand punch.

---

## Strengths

| Aspect | Assessment |
|--------|------------|
| **Terminal association** | The `>` prompt and window frame immediately signal CLI/TUI. Strong fit for the product. |
| **Brand elements** | "v" for vibe + "PDA" — clear, memorable, and on-brand. |
| **Target appeal** | Dark theme resonates with developers and power users. |
| **Professional look** | Clean, modern, not cluttered. Suitable for GitHub, docs, and app launchers. |

---

## Feedback & Recommended Improvements

### 1. Light-background visibility
**Issue:** On white/light backgrounds (GitHub, docs, app stores), the dark icon can lack contrast and feel flat.

**Recommendation:** Add a subtle outer stroke or light halo so the icon stands out on both dark and light backgrounds. Alternatively, provide a light-background variant.

**Status:** Applied — added subtle stroke and ensured sufficient contrast.

### 2. Scalability at small sizes
**Issue:** At 16×16 or 32×32 (favicon, tab icon), "PDA" text becomes illegible. The icon may blur into a dark blob.

**Recommendation:** Simplify the mark for small sizes: emphasize `>v` as the core symbol. Consider a favicon-specific 16×16 variant with only the prompt + "v".

**Status:** Primary icon optimized for 128–256px. Favicon variant documented for future use.

### 3. Brand punch — "v" vs "V"
**Issue:** Lowercase "v" feels casual; uppercase "V" can read as more authoritative and memorable.

**Recommendation:** Test both. For a "vibe" brand, lowercase can feel friendlier. For a productivity tool, uppercase may read as more professional. Current lowercase "v" is acceptable; consider A/B testing if rebranding.

**Status:** Kept lowercase "v" — aligns with "vibe" and differentiates from generic "V" logos.

### 4. Unique differentiator
**Issue:** Terminal + prompt is common. Adding a subtle PDA/card cue could improve memorability.

**Recommendation:** The "PDA" label helps. A faint card corner or list-line hint could reinforce "personal data assistant" without clutter. Optional enhancement.

**Status:** Deferred — current design is clean; avoid over-designing.

### 5. Color consistency
**Issue:** Blue accent (#4a9eff) should align with any existing brand palette (e.g., docs, screenshots).

**Recommendation:** Use the same blue as the app's title bar / highlights for consistency across touchpoints.

**Status:** Aligned with TUI blue tones from screenshot color scheme.

---

## Checklist (Post-Review)

| Item | Done |
|------|------|
| Icon works at 128–256px | ✓ |
| Readable on dark background | ✓ |
| Readable on light background | ✓ (stroke) |
| SVG is valid and scalable | ✓ |
| PNG exported at 256×256 | ✓ |
| README includes icon | ✓ |

---

## Regenerating the PNG

From the project root, with ImageMagick installed:

```bash
convert -background none -density 256 assets/icon.svg -resize 256x256 assets/icon.png
```

---

## Future Considerations

- **Favicon:** Add `assets/favicon.ico` (16×16, 32×32) for web/docs deployment.
- **App icon:** Provide 512×512 PNG for app stores or installers if needed.
- **Light variant:** Optional `icon-light.svg` for light-themed contexts.
