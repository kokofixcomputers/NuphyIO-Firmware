/** Firmware bytes -> "1.0.14.6" (same rule as the official app). */
export function parseFirmware(b: ArrayLike<number>): string {
  const base = `${b[2]}.${b[1]}.${b[0]}`;
  return b[3] === 170 ? `${base}.${b[4] | (b[5] << 8)}` : base;
}

/** Compare dotted versions; true when v >= min. */
export function versionAtLeast(v: string, min: string): boolean {
  const a = v.split(".").map(Number), b = min.split(".").map(Number);
  for (let i = 0; i < Math.max(a.length, b.length); i++) {
    const d = (a[i] ?? 0) - (b[i] ?? 0);
    if (d) return d > 0;
  }
  return true;
}
