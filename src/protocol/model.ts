/**
 * Which keyboard is this? The open V2-generation firmware (Air75 V2 and Halo75 V2) deliberately uses the V3's USB product ID (so nuphy.io accepts it), so the ID can't tell
 * them apart - but the USB product string can: a real V3 says "Air75 V3 ...", the V2 firmware says "Air75 V2 (open firmware)".
 */
export type ModelId = "air75v2" | "halo75v2" | "air96v2" | "air75v3" | "unknown";
export interface KbModel { id: ModelId; name: string; open: boolean; topRight: "knob" | "button" | "none" | "unknown"; numpad?: boolean }

export function identify(productName: string | undefined): KbModel {
  const n = (productName ?? "").trim();
  if (/air\s*75\s*v2/i.test(n)) return { id: "air75v2", name: "Air75 V2", open: /open/i.test(n), topRight: "button" };
  if (/halo\s*75\s*v2/i.test(n)) return { id: "halo75v2", name: "Halo75 V2", open: /open/i.test(n), topRight: "button" };
  if (/air\s*96\s*v2/i.test(n)) return { id: "air96v2", name: "Air96 V2", open: /open/i.test(n), topRight: "none", numpad: true };
  if (/air\s*75\s*v3/i.test(n)) return { id: "air75v3", name: "Air75 V3", open: false, topRight: "knob" };
  return { id: "unknown", name: n.replace(/\s*\(.*?\)\s*$/, "") || "NuPhy keyboard", open: false, topRight: "unknown" };
}
