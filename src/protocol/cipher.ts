export const hex = (b: number, w = 2): string => b.toString(16).toUpperCase().padStart(w, "0");
export const hexDump = (bytes: ArrayLike<number>, max = 64): string =>
  Array.from({ length: Math.min(bytes.length, max) }, (_, i) => hex(bytes[i])).join(" ");
