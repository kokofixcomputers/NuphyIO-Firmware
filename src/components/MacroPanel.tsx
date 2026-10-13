import { useEffect, useMemo, useRef, useState } from "react";
import { usageLabel } from "../protocol/hid";
import { entryAt } from "../protocol/keymap";
import { MACRO_CATEGORY_IDS } from "../protocol/keycodes";
import { KeyChip } from "./KeyPicker";
import { MACRO_DATA_START, MACRO_LIMIT, MACRO_SLOTS, MacroEvent, closeHeldKeys, decodeEvents, replaceMacro, splitMacros, textToMacro } from "../protocol/macro";
import { Nuphy } from "../useNuphy";
import { IconRec, IconTrash } from "./icons";
import { Card } from "./ui";

export default function MacroPanel({ nu }: { nu: Nuphy }) {
  const [slot, setSlot] = useState(0);
  const [events, setEvents] = useState<MacroEvent[]>([]);
  const [text, setText] = useState("hello world");
  const [delay, setDelay] = useState(40);
  const [recording, setRecording] = useState(false);
  const last = useRef(0);

  const lengths = useMemo(() => (nu.macros ? splitMacros(nu.macros).map((m) => m.length / 4) : []), [nu.macros]);

  const load = async (s = slot) => {
    const region = await nu.run((d) => d.readMacroRegion());
    if (region) { nu.setMacros(region); setEvents(decodeEvents(splitMacros(region)[s] ?? [])); }
  };
  useEffect(() => { void load(0); }, []); // eslint-disable-line react-hooks/exhaustive-deps

  const pick = (s: number) => {
    setSlot(s);
    setRecording(false);
    if (nu.macros) setEvents(decodeEvents(splitMacros(nu.macros)[s] ?? []));
  };

  // Hardware recorder: the keyboard stops typing into the OS and reports key events to us (the switch nuphy.io flips),
  // so you can press keys on the board itself - including ones the browser would never see (FN layers, media, Win/Cmd).
  const mapRef = useRef(nu.keymap); mapRef.current = nu.keymap;
  const layerRef = useRef(nu.lightMode); layerRef.current = nu.lightMode;
  useEffect(() => {
    if (!recording) return;
    last.current = 0;
    void nu.run((d) => d.startKeyUpload((s, down) => {
      const map = mapRef.current;
      const entry = map ? entryAt(map, layerRef.current * 4, s) : 0;
      nu.note("info", `key event: slot ${s} ${down ? "pressed" : "released"} (bound to 0x${entry.toString(16).padStart(4, "0")}${entry >> 8 !== 0 || entry === 0 || entry === 1 ? " - not a plain key, so it is not added to the macro" : ""})`);
      if (entry === 0 || entry === 1 || entry >> 8 !== 0) return;          // only plain keys can be macro events
      const now = performance.now();
      const gap = last.current ? Math.round(now - last.current) : 0;
      last.current = now;
      setEvents((ev) => [...ev, { down, key: entry & 0xff, delayMs: gap }]);
    }));
    return () => { void nu.run((d) => d.stopKeyUpload()); };
  }, [recording]); // eslint-disable-line react-hooks/exhaustive-deps

  const patch = (i: number, p: Partial<MacroEvent>) => setEvents((ev) => ev.map((e, j) => (j === i ? { ...e, ...p } : e)));

  const { fits, used } = useMemo(() => {
    try {
      const closed = closeHeldKeys(events);
      if (!nu.macros) return { fits: true, used: closed.length * 4 };
      const next = replaceMacro(nu.macros, slot, closed);
      const end = next[62] | (next[63] << 8);
      return { fits: true, used: end - MACRO_DATA_START };
    } catch { return { fits: false, used: MACRO_LIMIT } }
  }, [events, nu.macros, slot]);
  const capacity = MACRO_LIMIT - MACRO_DATA_START;

  const write = async () => {
    const region = await nu.run((d) => d.writeMacro(slot, closeHeldKeys(events)), `Macro ${slot + 1} written`);
    if (region) { nu.setMacros(region); setEvents(decodeEvents(splitMacros(region)[slot] ?? [])); }
  };
  const clear = async () => {
    const region = await nu.run((d) => d.writeMacro(slot, []), `Macro ${slot + 1} cleared`);
    if (region) { nu.setMacros(region); setEvents([]); }
  };

  return (
    <div className="grid macro-grid">
      <Card title="Slots" className="slots">
        <ul className="slot-list">
          {Array.from({ length: MACRO_SLOTS }, (_, i) => (
            <li key={i}>
              <button className={`slot${slot === i ? " on" : ""}`} onClick={() => pick(i)}>
                <span className="slot-id mono">M{i + 1}</span>
                <span className="slot-name">Macro {i + 1}</span>
                {lengths[i] > 0 ? <span className="slot-count mono">{lengths[i]} ev</span> : <span className="slot-count faint">empty</span>}
              </button>
            </li>
          ))}
        </ul>
      </Card>

      <Card
        title={`Macro ${slot + 1}`}
        aside={
          <div className="row gap">
            <button className="btn ghost sm" onClick={() => void load(slot)}>Read from keyboard</button>
            <button className={`btn sm ${recording ? "rec" : "ghost"}`} onClick={() => setRecording((r) => !r)}>
              <IconRec /> {recording ? "Stop" : "Record"}
            </button>
          </div>
        }
      >
        {recording && <div className="callout info"><i className="pulse" /> Recording on the keyboard — press keys on the board itself. Nothing is typed into your computer meanwhile. (Every key press is also listed in the Console tab with its slot number, which shows whether the board sees a key at all.)</div>}

        <div className="compose">
          <textarea rows={2} value={text} onChange={(e) => setText(e.target.value)} placeholder="Type text to turn into key events…" />
          <label className="field inline">Delay <input type="number" min={0} max={65535} value={delay} onChange={(e) => setDelay(+e.target.value)} /> ms</label>
          <button className="btn ghost sm" onClick={() => setEvents(textToMacro(text, delay))}>Generate</button>
        </div>

        <div className="table-wrap">
          <table className="events">
            <thead><tr><th>#</th><th>Key</th><th>State</th><th>Delay</th><th /></tr></thead>
            <tbody>
              {events.length === 0 && <tr><td colSpan={5} className="empty-row">No events. Record keystrokes, generate from text, or add one.</td></tr>}
              {events.map((e, i) => (
                <tr key={i}>
                  <td className="mono faint">{String(i + 1).padStart(2, "0")}</td>
                  <td>
                    <KeyChip label={usageLabel(e.key)} categories={MACRO_CATEGORY_IDS} onPick={(entry) => patch(i, { key: entry & 0xff })} />
                  </td>
                  <td><button className={`pill-btn ${e.down ? "down" : "up"}`} onClick={() => patch(i, { down: !e.down })}>{e.down ? "↓ press" : "↑ release"}</button></td>
                  <td><span className="num"><input type="number" min={0} max={65535} value={e.delayMs} onChange={(ev) => patch(i, { delayMs: +ev.target.value })} /><i>ms</i></span></td>
                  <td><button className="icon-btn" aria-label="Remove event" onClick={() => setEvents((ev) => ev.filter((_, j) => j !== i))}><IconTrash /></button></td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>

        <div className="macro-foot">
          <div className="meter" title="Shared by all macros">
            <div className="meter-bar"><i style={{ width: `${Math.min(100, (used / capacity) * 100)}%` }} className={fits ? "" : "over"} /></div>
            <span className="mono">{used} / {capacity} bytes {fits ? "" : "· too large"}</span>
          </div>
          <div className="row gap">
            <button className="btn ghost sm" onClick={() => setEvents((ev) => [...ev, { down: true, key: 0x04, delayMs: delay }])}>+ Event</button>
            <button className="btn ghost sm" disabled={!nu.macros} onClick={() => void clear()}>Clear slot</button>
            <button className="btn primary" disabled={!fits || events.length === 0} onClick={() => void write()}>Write to keyboard</button>
          </div>
        </div>
        <p className="hint">Bind a macro to a key from the Keys tab (Inspector → Macro).</p>
      </Card>
    </div>
  );
}
