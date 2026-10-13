import { useEffect, useState } from "react";
import { LAYERS, describeEntry, entryAt, setLabelOS } from "../protocol/keymap";
import { Nuphy } from "../useNuphy";
import KeyboardView from "./KeyboardView";
import KnobTargets from "./KnobTargets";
import { BUTTON_SLOT, layoutFor } from "../layout";
import KeyPicker from "./KeyPicker";
import { keyIcon } from "./keyIcons";
import { Card, Segmented } from "./ui";

const hex4 = (n: number) => "0x" + n.toString(16).padStart(4, "0");

export default function KeysPanel({ nu }: { nu: Nuphy }) {
  const [win, setWin] = useState(nu.activeMode === 1);
  useEffect(() => { setWin(nu.activeMode === 1); }, [nu.activeMode]);   // follow the keyboard's Mac / Windows switch
  const [fn, setFn] = useState(0);
  const [sel, setSel] = useState<number | null>(null);
  const [raw, setRaw] = useState("0x0001");
  const [busy, setBusy] = useState(false);

  // Key test: the keyboard stops typing and reports every key it sees; the board on screen lights the pressed keys.
  const [testing, setTesting] = useState(false);
  const [down, setDown] = useState<ReadonlySet<number>>(new Set());
  const [seen, setSeen] = useState<number | null>(null);
  const [events, setEvents] = useState<string[]>([]);
  useEffect(() => {
    if (!testing) { setDown(new Set()); return; }
    void nu.run((d) => d.startKeyUpload((slot, isDown) => {
      setSeen(slot);
      setEvents((l) => [...l.slice(-11), `slot ${slot} ${isDown ? "down" : "up"}`]);
      setDown((s) => { const n = new Set(s); if (isDown) n.add(slot); else n.delete(slot); return n; });
    }));
    return () => { void nu.run((d) => d.stopKeyUpload()); };
  }, [testing]); // eslint-disable-line react-hooks/exhaustive-deps

  setLabelOS(win);
  const layer = (win ? 4 : 0) + fn;
  const map = nu.keymap;
  const entry = (slot: number) => (map ? entryAt(map, layer, slot) : 0);
  const cur = sel === null ? null : entry(sel);

  const write = async (e: number) => {
    if (sel === null) { nu.toast("info", "Select a key on the keyboard first"); return; }
    if (busy) return;
    setBusy(true);
    const before = describeEntry(entry(sel)) || "empty";
    const back = await nu.run((d) => d.writeKey(layer, sel, e));
    if (back !== undefined) {
      nu.patchKey(layer, sel, back);
      nu.toast(back === e ? "ok" : "error", back === e ? `${before} → ${describeEntry(back) || hex4(back)} · written and verified` : `Wrote ${hex4(e)} but the board reports ${hex4(back)}`);
    }
    setBusy(false);
  };

  const resetDefault = async () => {
    if (sel === null) return;
    const def = await nu.run((d) => d.readDefaultKey(layer, sel));
    if (def !== undefined) await write(def);
  };

  const parsedRaw = (() => { const v = parseInt(raw, raw.trim().toLowerCase().startsWith("0x") ? 16 : 10); return Number.isNaN(v) || v < 0 || v > 0xffff ? null : v; })();

  return (
    <div className="grid keys-grid">
      <div className="stack">
        <Card
          title="Layout"
          aside={
            <div className="row gap">
              <button className={`btn sm ${testing ? "rec" : "ghost"}`} onClick={() => setTesting((t) => !t)}>{testing ? "Stop key test" : "Key test"}</button>
              <Segmented value={win ? "win" : "mac"} onChange={(v) => setWin(v === "win")} options={[{ value: "mac", label: "Mac" }, { value: "win", label: "Windows" }]} />
              <Segmented value={fn} onChange={setFn} options={Array.from({ length: LAYERS / 2 }, (_, i) => ({ value: i, label: `FN ${i}` }))} />
            </div>
          }
        >
          <KeyboardView layout={layoutFor(nu.model.numpad)} accessory={nu.model.topRight === "none" ? undefined : nu.accessory} color={testing ? (s) => (down.has(s) ? "var(--accent)" : undefined) : undefined} selected={sel} label={(s) => describeEntry(entry(s))} icon={(s) => keyIcon(entry(s))} onSelect={setSel} dim={(s) => entry(s) === 1} />
          {nu.model.topRight !== "none" && <div className="accessory-pick">
            <span className="hint nm">Top-right</span>
            <Segmented size="sm" value={nu.accessory} onChange={(v) => void nu.setAccessory(v)} options={[{ value: "knob", label: "Knob" }, { value: "button", label: "Button" }]} />
          </div>}
          {nu.model.topRight === "none" ? null : nu.accessory === "knob" ? <KnobTargets sel={sel} onSelect={setSel} entry={entry} /> : <p className="hint">The top-right button is slot {BUTTON_SLOT}: click it on the board, then pick what it should do.</p>}
          {testing && <div className="callout info"><i className="pulse" /><div><b>Key test is on.</b> {events.length > 0 && <span className="mono">[{events.join(" · ")}]</span>} The keyboard is not typing into your computer. Press keys: they light up here. {seen === null ? "Nothing seen yet." : `Last key seen: slot ${seen}${entry(seen) ? ` (bound to ${describeEntry(entry(seen)) || hex4(entry(seen))})` : ""}.`}</div></div>}
          <p className="hint">1 · Click a key on the board. 2 · Click a key in the library below to assign it. <span className="swatch tr">▽</span> keys fall through to the layer below.</p>
        </Card>

        <Card title="Key library" aside={<span className="hint nm">{sel === null ? "Select a key above first" : `Assigning to slot ${sel}`}</span>}>
          <KeyPicker value={cur} win={win} keymap={map} withLayers withMacros onPick={(e) => void write(e)} />
        </Card>
      </div>

      <Card title="Inspector" className="inspector">
        {sel === null || cur === null ? (
          <div className="empty"><b>No key selected</b><p>Pick a key on the board to see and change what it does on layer <span className="mono">{layer}</span>.</p></div>
        ) : (
          <>
            <div className="readout">
              <div className="cap">{keyIcon(cur) ?? (describeEntry(cur) || "—")}</div>
              <dl>
                <div><dt>Slot</dt><dd className="mono">{sel}</dd></div>
                <div><dt>Entry</dt><dd className="mono">{hex4(cur)}</dd></div>
                <div><dt>Layer</dt><dd className="mono">{layer}</dd></div>
              </dl>
            </div>
            <button className="btn ghost sm" disabled={busy} onClick={() => void resetDefault()}>Reset to factory default</button>
            <details className="adv">
              <summary>Raw 16-bit entry</summary>
              <div className="row gap">
                <input className="mono" value={raw} onChange={(e) => setRaw(e.target.value)} spellCheck={false} />
                <button className="btn sm" disabled={busy || parsedRaw === null} onClick={() => parsedRaw !== null && void write(parsedRaw)}>Write</button>
              </div>
            </details>
          </>
        )}
        <button className="btn ghost sm reread" onClick={() => void nu.run(async (d) => nu.setKeymap(await d.readKeymap()), "Keymap re-read from keyboard")}>Re-read keymap</button>
      </Card>
    </div>
  );
}
