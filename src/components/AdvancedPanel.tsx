import { useEffect, useMemo, useState } from "react";
import { TD_DEFAULT_TERM, TD_SLOTS, TGL_SLOTS, TapDance, entryToTd, entryToTgl, parseTapDance, parseTgl, tdEntry, tglEntry } from "../protocol/advanced";
import { LAYERS, describeEntry, entryAt, setLabelOS } from "../protocol/keymap";
import { ModeSettings, SleepCfg, StoreStatus } from "../protocol/device";
import { Nuphy } from "../useNuphy";
import KeyboardView from "./KeyboardView";
import { layoutFor } from "../layout";
import KeyPicker from "./KeyPicker";
import { keyIcon } from "./keyIcons";
import { Card, Segmented, Slider } from "./ui";

type Kind = "td" | "tgl";
type Field = "tap" | "double" | "hold";
const FIELDS: { id: Field; title: string; hint: string }[] = [
  { id: "tap", title: "Tap", hint: "single press" },
  { id: "double", title: "Double tap", hint: "two quick presses" },
  { id: "hold", title: "Hold", hint: "press and hold" },
];
const layerName = (l: number) => `${l < 4 ? "Mac" : "Win"} FN${l % 4}`;

export default function AdvancedPanel({ nu }: { nu: Nuphy }) {
  const [kind, setKind] = useState<Kind>("td");
  const [td, setTd] = useState<(TapDance | null)[]>([]);
  const [tgl, setTgl] = useState<(number | null)[]>([]);
  const [slot, setSlot] = useState(0);
  const [draft, setDraft] = useState<TapDance>({ tap: 0, double: 0, hold: 0, termMs: TD_DEFAULT_TERM });
  const [draftTgl, setDraftTgl] = useState(0);
  const [field, setField] = useState<Field>("tap");
  const [win, setWin] = useState(nu.activeMode === 1);
  useEffect(() => { setWin(nu.activeMode === 1); }, [nu.activeMode]);   // follow the keyboard's Mac / Windows switch
  const [fn, setFn] = useState(0);
  const [target, setTarget] = useState<number | null>(null);
  const [busy, setBusy] = useState(false);
  const [sleep, setSleep] = useState<SleepCfg | null>(null);
  const [mode, setMode] = useState<ModeSettings | null>(null);
  const saveMode = async (m: ModeSettings) => { setMode(m); const back = await nu.run((d) => d.writeModeSettings(m), "Mode settings saved"); if (back) setMode(back); };
  const [store, setStore] = useState<StoreStatus | null>(null);
  const refreshStore = async () => { const s = await nu.run((d) => d.readStoreStatus()); if (s) setStore(s); };
  setLabelOS(win);
  const layer = (win ? 4 : 0) + fn;
  const map = nu.keymap;

  const load = async () => {
    const r = await nu.run((d) => d.readAdvanced());
    if (!r) return;
    setTd(Array.from({ length: TD_SLOTS }, (_, i) => parseTapDance(r.td.subarray(i * 8, i * 8 + 8))));
    setTgl(Array.from({ length: TGL_SLOTS }, (_, i) => parseTgl(r.tgl.subarray(i * 2, i * 2 + 2))));
  };
  useEffect(() => { if (nu.sleepRev) void nu.run((d) => d.readSleep()).then((c) => c && setSleep(c)); }, [nu.sleepRev]); // eslint-disable-line react-hooks/exhaustive-deps
  useEffect(() => { void nu.run((d) => d.readSleep()).then((c) => c && setSleep(c)); void refreshStore(); void nu.run((d) => d.readModeSettings()).then((m) => m && setMode(m)); }, []); // eslint-disable-line react-hooks/exhaustive-deps
  const saveSleep = async (c: SleepCfg) => {
    setSleep(c);
    const back = await nu.run((d) => d.writeSleep(c), "Sleep settings saved");
    if (back) setSleep(back);
    setTimeout(() => void refreshStore(), 900);
  };
  useEffect(() => { void load(); }, []); // eslint-disable-line react-hooks/exhaustive-deps

  // Load the chosen slot into the editor.
  useEffect(() => {
    if (kind === "td") setDraft(td[slot] ?? { tap: 0, double: 0, hold: 0, termMs: TD_DEFAULT_TERM });
    else setDraftTgl(tgl[slot] ?? 0);
  }, [slot, kind, td, tgl]);

  // Where is each slot bound?
  const bindings = useMemo(() => {
    const out = new Map<string, { layer: number; slot: number }[]>();
    if (!map) return out;
    for (let l = 0; l < LAYERS; l++) for (let s = 0; s < 98; s++) {
      const e = entryAt(map, l, s);
      const idx = kind === "td" ? entryToTd(e) : entryToTgl(e);
      if (idx !== null) out.set(String(idx), [...(out.get(String(idx)) ?? []), { layer: l, slot: s }]);
    }
    return out;
  }, [map, kind]);
  const bound = bindings.get(String(slot)) ?? [];

  const lbl = (e: number) => (e === 0 ? "—" : describeEntry(e) || "?");
  const slotSummary = (i: number) => {
    if (kind === "td") { const t = td[i]; return t ? `${lbl(t.tap)} · ${lbl(t.double)} · ${lbl(t.hold)}` : "free"; }
    const t = tgl[i]; return t === null || t === undefined ? "free" : lbl(t);
  };
  const count = kind === "td" ? TD_SLOTS : TGL_SLOTS;

  const pickKey = (e: number) => {
    if (kind === "tgl") { setDraftTgl(e); return; }
    setDraft((d) => ({ ...d, [field]: e }));
    setField((f) => (f === "tap" ? "double" : f === "double" ? "hold" : "hold"));
  };

  const saveSlot = async (bind: boolean) => {
    if (busy) return;
    if (bind && target === null) { nu.toast("info", "Click the key you want to bind on the board first"); return; }
    setBusy(true);
    const wrote = await nu.run((d) => (kind === "td" ? d.writeTapDance(slot, draft) : d.writeTgl(slot, draftTgl)));
    if (wrote) {
      if (kind === "td") setTd((a) => a.map((v, i) => (i === slot ? { ...draft } : v)));
      else setTgl((a) => a.map((v, i) => (i === slot ? draftTgl : v)));
      if (bind && target !== null) {
        const want = kind === "td" ? tdEntry(slot) : tglEntry(slot);
        const back = await nu.run((d) => d.writeKey(layer, target, want));
        if (back !== undefined) {
          nu.patchKey(layer, target, back);
          nu.toast(back === want ? "ok" : "error", back === want ? `${kind === "td" ? "Tap Dance" : "Toggle"} ${slot + 1} saved and bound to ${layerName(layer)} · slot ${target}` : "Slot saved, but the key reads back differently");
        }
      } else nu.toast("ok", `${kind === "td" ? "Tap Dance" : "Toggle"} ${slot + 1} saved`);
    }
    setBusy(false);
  };

  const freeSlot = async () => {
    if (busy) return;
    setBusy(true);
    for (const b of bound) {
      const def = await nu.run((d) => d.readDefaultKey(b.layer, b.slot));
      if (def !== undefined) { const back = await nu.run((d) => d.writeKey(b.layer, b.slot, def)); if (back !== undefined) nu.patchKey(b.layer, b.slot, back); }
    }
    const ok = await nu.run((d) => (kind === "td" ? d.writeTapDance(slot, null) : d.writeTgl(slot, null)), `Slot ${slot + 1} freed${bound.length ? " and its keys restored to default" : ""}`);
    if (ok) { if (kind === "td") setTd((a) => a.map((v, i) => (i === slot ? null : v))); else setTgl((a) => a.map((v, i) => (i === slot ? null : v))); }
    setBusy(false);
  };

  const unbind = async (b: { layer: number; slot: number }) => {
    const def = await nu.run((d) => d.readDefaultKey(b.layer, b.slot));
    if (def === undefined) return;
    const back = await nu.run((d) => d.writeKey(b.layer, b.slot, def), `Restored ${layerName(b.layer)} · slot ${b.slot} to default`);
    if (back !== undefined) nu.patchKey(b.layer, b.slot, back);
  };

  return (
    <div className="grid macro-grid">
      {mode && (
        <Card title="Key lock-outs" className="sleep-card">
          <p className="hint">Keys the keyboard refuses to send, for gaming. Windows and Alt+F4 / Alt+Tab apply on the Windows layers; the debounce level trades speed for chatter protection.</p>
          {([["disableWin", "Disable Windows key"], ["disableAltF4", "Disable Alt+F4"], ["disableAltTab", "Disable Alt+Tab"]] as const).map(([k, label]) => (
            <label className="row gap between" key={k}>
              <span>{label}</span>
              <button role="switch" aria-checked={mode[k]} aria-label={label} className={`switch sm${mode[k] ? " on" : ""}`} onClick={() => void saveMode({ ...mode, [k]: !mode[k] })}><i /></button>
            </label>
          ))}
          <div className="row gap between"><span>Key stabilization</span>
            <Segmented size="sm" value={mode.stabilization <= 1 ? 1 : mode.stabilization >= 3 ? 3 : 2} onChange={(v) => void saveMode({ ...mode, stabilization: v })} options={[{ value: 1, label: "Low" }, { value: 2, label: "Medium" }, { value: 3, label: "High" }]} />
          </div>
        </Card>
      )}
      {sleep && (
        <Card title="Sleep" className="sleep-card" >
          <div className="row gap wrap">
            <Segmented value={sleep.enabled ? "on" : "off"} onChange={(v) => void saveSleep({ ...sleep, enabled: v === "on" })} options={[{ value: "on", label: "Auto sleep on" }, { value: "off", label: "Off" }]} />
          </div>
          <p className="hint">Level 1 turns the lighting off after the keyboard has been idle; level 2 also shuts down communication to save battery.</p>
          <Slider label="Level 1 — lights off" value={sleep.level1} min={1} max={120} unit=" min" onChange={(v) => setSleep({ ...sleep, level1: v, level2: Math.max(sleep.level2, v) })} />
          <Slider label="Level 2 — deep sleep" value={sleep.level2} min={sleep.level1} max={120} unit=" min" onChange={(v) => setSleep({ ...sleep, level2: v })} />
          <div className="row gap wrap">
            <label className="field inline">Level 1 <input type="number" min={1} max={120} value={sleep.level1} onChange={(e) => { const v = Math.min(120, Math.max(1, +e.target.value || 1)); setSleep({ ...sleep, level1: v, level2: Math.max(sleep.level2, v) }); }} /> min</label>
            <label className="field inline">Level 2 <input type="number" min={sleep.level1} max={120} value={sleep.level2} onChange={(e) => setSleep({ ...sleep, level2: Math.min(120, Math.max(sleep.level1, +e.target.value || sleep.level1)) })} /> min</label>
          </div>
          {store && (
            <p className="hint">
              Flash store: {store.pending ? "saving…" : "everything saved"} · {store.loadedAtBoot ? "restored saved settings at power-up" : "no saved settings found at power-up (defaults)"} · {store.appends} change{store.appends === 1 ? "" : "s"} logged, {store.compactions} bank swap{store.compactions === 1 ? "" : "s"}, {store.freeKB} KB log space left{store.failures ? ` · ${store.failures} WRITE FAILURES` : ""}
            </p>
          )}
          <div className="row end foot"><button className="btn primary" onClick={() => void saveSleep(sleep)}>Save sleep settings</button></div>
        </Card>
      )}
      <Card title="Slots" className="slots">
        <div className="slots-head"><Segmented size="sm" value={kind} onChange={(v) => { setKind(v); setSlot(0); }} options={[{ value: "td", label: "Tap Dance" }, { value: "tgl", label: "Toggle" }]} /></div>
        <ul className="slot-list">
          {Array.from({ length: count }, (_, i) => {
            const nb = bindings.get(String(i))?.length ?? 0;
            return (
              <li key={i}>
                <button className={`slot${slot === i ? " on" : ""}`} onClick={() => setSlot(i)}>
                  <span className="slot-id mono">{kind === "td" ? "TD" : "TG"}{i + 1}</span>
                  <span className="slot-name">{slotSummary(i)}</span>
                  {nb > 0 && <span className="slot-count mono">{nb}×</span>}
                </button>
              </li>
            );
          })}
        </ul>
      </Card>

      <div className="stack">
        <Card title={`${kind === "td" ? "Tap Dance" : "Toggle Key"} ${slot + 1}`}>
          <p className="hint tight">
            {kind === "td"
              ? "One key, three behaviours: a single tap, a double tap, or a long press each send a different key."
              : "Tap the key to lock an action on, tap again to turn it off. Hold the key for its normal behaviour."}
          </p>

          {kind === "td" ? (
            <div className="targets">
              {FIELDS.map((f) => (
                <button key={f.id} className={`target${field === f.id ? " on" : ""}`} onClick={() => setField(f.id)}>
                  <span className="cap">{lbl(draft[f.id])}</span>
                  <b>{f.title}</b><small>{f.hint}</small>
                </button>
              ))}
            </div>
          ) : (
            <div className="targets one"><div className="target on"><span className="cap">{lbl(draftTgl)}</span><b>Toggles</b><small>pick a key below</small></div></div>
          )}

          {kind === "td" && (
            <div className="term"><Slider label="Hold / double-tap window" value={draft.termMs} min={50} max={500} step={10} unit=" ms" onChange={(v) => setDraft((d) => ({ ...d, termMs: v }))} /></div>
          )}

          <KeyPicker compact value={kind === "td" ? draft[field] : draftTgl} onPick={pickKey} categories={["basic", "numpad", "media"]} />
        </Card>

        <Card
          title="Bind to a key"
          aside={
            <div className="row gap">
              <Segmented size="sm" value={win ? "win" : "mac"} onChange={(v) => setWin(v === "win")} options={[{ value: "mac", label: "Mac" }, { value: "win", label: "Windows" }]} />
              <Segmented size="sm" value={fn} onChange={setFn} options={Array.from({ length: 4 }, (_, i) => ({ value: i, label: `FN${i}` }))} />
            </div>
          }
        >
          <KeyboardView
            layout={layoutFor(nu.model.numpad)}
            accessory={nu.model.topRight === "none" ? undefined : nu.accessory}
            selected={target}
            onSelect={setTarget}
            label={(s) => describeEntry(map ? entryAt(map, layer, s) : 0)}
            icon={(s) => keyIcon(map ? entryAt(map, layer, s) : 0)}
          />
          {bound.length > 0 && (
            <div className="bound">
              <span className="hint nm">Currently bound to</span>
              <div className="chips">
                {bound.map((b) => (
                  <span key={`${b.layer}-${b.slot}`} className="chip static">{layerName(b.layer)} · slot {b.slot}<button aria-label="Unbind" onClick={() => void unbind(b)}>✕</button></span>
                ))}
              </div>
            </div>
          )}
          <div className="row gap wrap end foot">
            <button className="btn ghost sm" disabled={busy} onClick={() => void freeSlot()}>Free slot</button>
            <button className="btn ghost sm" disabled={busy} onClick={() => void saveSlot(false)}>Save slot only</button>
            <button className="btn primary" disabled={busy || target === null} onClick={() => void saveSlot(true)}>
              {busy ? "Writing…" : target === null ? "Pick a key to bind" : `Save & bind to ${describeEntry(map ? entryAt(map, layer, target) : 0) || `slot ${target}`}`}
            </button>
          </div>
        </Card>
      </div>
    </div>
  );
}
