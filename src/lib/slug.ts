/** Lowercase, diacritics stripped – used for search matching (ä → a, ō → o, …). */
export function norm(s: string): string {
  return s
    .toLowerCase()
    .replace(/ß/g, "ss")
    .normalize("NFKD")
    .replace(/[\u0300-\u036f]/g, "")
    .replace(/\s+/g, " ")
    .trim();
}

/** URL-safe slug for anchors: "Ino-Shika-Chō" → "ino-shika-cho". */
export function slug(s: string): string {
  return norm(s)
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "");
}
