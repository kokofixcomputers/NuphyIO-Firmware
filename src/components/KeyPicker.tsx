import { useEffect, useMemo, useRef, useState } from "react";
import { CATEGORIES, KeyDef } from "../protocol/keycodes";
import { describeEntry, entryAt, osLabel } from "../protocol/keymap";
import { keyIcon } from "./keyIcons";
import { Segmented } from "./ui";

interface Props {
  onPick: (entry: number) => void;
  value?: number | null;
  win?: boolean; // which mode's FN layers to offer
  keymap?: Uint8Array | null;
  categories?: string[]; // restrict categories (macros only take 8-bit keys)
  withLayers?: boolean;
  withMacros?: boolean;
  compact?: boolean;
}

const hex4 = (n: number) => "0x" + n.toString(16).padStart(4, "0");

export default function KeyPicker({ onPick, value, win = false, keymap, categories, withLayers = false, withMacros = false, compact = false }: Props) {
  const cats = useMemo(() => {
    const base = CATEGORIES.filter((c) => !categories || categories.includes(c.id));
    const out = base.map((c) => ({ ...c, groups: [...c.groups] }));
    const layerBase = win ? 4 : 0;
    if (withLayers) {
      out.push({
        id: "layers", label: "Layers", groups: [{
          title: `Momentary layers (${win ? "Windows" : "Mac"})`,
          keys: [1, 2, 3].map((n) => ({ entry: 0x5220 | (layerBase + n), label: `FN ${n}`, name: `Hold for layer FN ${n}` })),
        }, {
          title: "Behaviour",
          keys: [{ entry: 0x0001, label: "▽ Trans", name: "Transparent: use the layer below" }, { entry: 0x0000, label: "None", name: "Does nothing" }],
        }],
      });
    }
    if (withMacros) {
      out.push({ id: "macro", label: "Macro", groups: [{ title: "Macro slots", keys: Array.from({ length: 31 }, (_, i) => ({ entry: 0x7700 | i, label: `M${i + 1}`, name: `Run macro ${i + 1}` })) }] });
    }
    if (keymap && !categories) {
      const seen = new Map<number, KeyDef>();
      for (let l = 0; l < 8; l++) for (let s = 0; s < 98; s++) {
        const e = entryAt(keymap, l, s);
        const hi = e >> 8;
        if (hi === 0x7e || hi === 0x78) if (!seen.has(e) && !CATEGORIES.find((c) => c.id === "special")!.groups[0].keys.some((x) => x.entry === e)) seen.set(e, { entry: e, label: describeEntry(e), name: `Entry ${hex4(e)} found in your keymap` });
      }
      if (seen.size) {
        const special = out.find((c) => c.id === "special");
        const g = { title: "From your keymap", keys: [...seen.values()].sort((a, b) => a.entry - b.entry) };
        if (special) special.groups.push(g); else out.push({ id: "special", label: "Special", groups: [g] });
      }
    }
    return out;
  }, [categories, win, keymap, withLayers, withMacros]);

  const [cat, setCat] = useState(cats[0]?.id ?? "basic");
  const [q, setQ] = useState("");
  const active = cats.find((c) => c.id === cat) ?? cats[0];
  const needle = q.trim().toLowerCase();

  // Searching spans every category.
  const groups = needle
    ? cats.flatMap((c) => c.groups).map((g) => ({ ...g, keys: g.keys.filter((x) => (x.label + " " + x.name).toLowerCase().includes(needle)) })).filter((g) => g.keys.length)
    : active.groups;

  return (
    <div className={`picker${compact ? " compact" : ""}`}>
      <div className="picker-bar">
        <Segmented size="sm" value={cat} onChange={(v) => { setCat(v); setQ(""); }} options={cats.map((c) => ({ value: c.id, label: c.label }))} />
        <input className="picker-search" placeholder="Search keys…" value={q} onChange={(e) => setQ(e.target.value)} />
      </div>
      <div className="picker-body">
        {groups.length === 0 && <p className="hint">No keys match “{q}”.</p>}
        {groups.map((g) => (
          <div key={g.title}>
            <h5>{g.title}</h5>
            <div className="pk-grid">
              {g.keys.map((d) => (
                <button key={d.entry} className={`pk${value === d.entry ? " on" : ""}${keyIcon(d.entry) ? " ico" : d.label.length > 5 ? " wide" : ""}`} title={`${d.name} · ${hex4(d.entry)}`} onClick={() => onPick(d.entry)}>
                  {keyIcon(d.entry) ? <><span className="pk-i">{keyIcon(d.entry)}</span><span className="pk-l">{osLabel(d.label, win)}</span></> : osLabel(d.label, win)}
                </button>
              ))}
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}

/** A key chip that opens a compact picker in a popover (used where a dropdown would have been). */
export function KeyChip({ label, onPick, categories, align = "left" }: { label: string; onPick: (entry: number) => void; categories?: string[]; align?: "left" | "right" }) {
  const [open, setOpen] = useState(false);
  const ref = useRef<HTMLDivElement>(null);
  useEffect(() => {
    if (!open) return;
    const close = (e: MouseEvent) => { if (!ref.current?.contains(e.target as Node)) setOpen(false); };
    const esc = (e: KeyboardEvent) => { if (e.key === "Escape") setOpen(false); };
    document.addEventListener("mousedown", close);
    document.addEventListener("keydown", esc);
    return () => { document.removeEventListener("mousedown", close); document.removeEventListener("keydown", esc); };
  }, [open]);
  return (
    <div className="chip-pop" ref={ref}>
      <button className="keychip" onClick={() => setOpen((o) => !o)}>{label}</button>
      {open && (
        <div className={`pop ${align}`}>
          <KeyPicker compact categories={categories} onPick={(e) => { onPick(e); setOpen(false); }} />
        </div>
      )}
    </div>
  );
}
