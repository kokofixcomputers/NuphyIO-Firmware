import { ReactNode, useCallback, useEffect, useRef, useState } from "react";
import { DEMO } from "./demo";
import { useNuphy } from "./useNuphy";
import { NuPhyDevice } from "./protocol/device";
import Console from "./components/Console";
import AdvancedPanel from "./components/AdvancedPanel";
import KeysPanel from "./components/KeysPanel";
import Landing from "./components/Landing";
import LightingPanel from "./components/LightingPanel";
import MacroPanel from "./components/MacroPanel";
import DeviceMenu, { DeviceSummary } from "./components/DeviceMenu";
import Toasts from "./components/Toasts";
import { IconAdvanced, IconAlert, IconConsole, IconKeys, IconLight, IconMacro } from "./components/icons";
import { parseFirmware } from "./protocol/info";

type Tab = "keys" | "advanced" | "lighting" | "macros" | "console";
const TABS: { id: Tab; label: string; sub: string; icon: ReactNode }[] = [
  { id: "keys", label: "Keys", sub: "Remap any key on any layer", icon: <IconKeys /> },
  { id: "advanced", label: "Advanced", sub: "Tap Dance and Toggle keys", icon: <IconAdvanced /> },
  { id: "lighting", label: "Lighting", sub: "Effects, colour and a live view of the LEDs", icon: <IconLight /> },
  { id: "macros", label: "Macros", sub: "Record, edit and store key sequences", icon: <IconMacro /> },
  { id: "console", label: "Console", sub: "Every frame on the wire", icon: <IconConsole /> },
];

const Mark = () => (
  <svg width="30" height="30" viewBox="0 0 32 32" aria-hidden>
    <rect x="1.5" y="1.5" width="29" height="29" rx="9" fill="none" stroke="currentColor" strokeOpacity=".35" />
    <rect x="7" y="9" width="18" height="14" rx="3.5" fill="var(--accent)" />
    <path d="M11 14.5h.01M15 14.5h.01M19 14.5h.01M12 19h8" stroke="#0b0d05" strokeWidth="2" strokeLinecap="round" />
  </svg>
);

interface Entry { id: number; hid: HIDDevice }

/** One keyboard: owns its connection and state, stays alive (and connected) while another keyboard is on screen. */
function Session({ entry, active, devices, onSummary, onSelect, onAdd, onRemove }: {
  entry: Entry; active: boolean; devices: DeviceSummary[];
  onSummary: (s: DeviceSummary) => void; onSelect: (id: number) => void; onAdd: () => void; onRemove: (id: number) => void;
}) {
  const nu = useNuphy();
  const [tab, setTab] = useState<Tab>("keys");
  const connected = nu.status === "connected" || nu.status === "reconnecting";
  const started = useRef(false);
  const wasConnected = useRef(false);
  if (connected) wasConnected.current = true;

  useEffect(() => { if (started.current) return; started.current = true; void nu.connect(entry.hid); }, []); // eslint-disable-line react-hooks/exhaustive-deps

  const name = nu.model.id === "unknown" ? (nu.productName || entry.hid.productName || "NuPhy keyboard") : nu.model.name;
  const model = nu.productName ? nu.model.id : "unknown";
  const detail = connected ? `${nu.activeMode === 1 ? "Windows" : "macOS"}${nu.model.open ? " · open FW" : ""}` : undefined;
  useEffect(() => { onSummary({ id: entry.id, name, model, status: nu.status, detail }); }, [entry.id, name, model, nu.status, detail]); // eslint-disable-line react-hooks/exhaustive-deps

  useEffect(() => {
    if (!connected || !active) return;
    const onKey = (e: KeyboardEvent) => {
      const t = e.target as HTMLElement;
      if (e.metaKey || e.ctrlKey || e.altKey || /INPUT|SELECT|TEXTAREA/.test(t.tagName)) return;
      const tab = TABS[Number(e.key) - 1];
      if (tab) setTab(tab.id);
    };
    window.addEventListener("keydown", onKey);
    return () => { window.removeEventListener("keydown", onKey); };
  }, [connected, active]);

  if (!active) return null;
  const menu = <DeviceMenu devices={devices} activeId={entry.id} onSelect={onSelect} onAdd={onAdd} onRemove={onRemove} />;

  if (!connected) {
    return (
      <div className="app">
        <aside className="rail"><div className="brand"><Mark /><div><b>Air Deck</b><span>NuPhy keyboard control</span></div></div>{menu}</aside>
        <main>
          <div className="session-state">
            {nu.status === "error" || (nu.status === "idle" && wasConnected.current) ? (
              <>
                <div className="callout error"><IconAlert /><div><b>{nu.status === "error" ? `Couldn't connect to ${name}.` : `${name} was disconnected.`}</b> {nu.error ?? "Unplugged, or another tab took it."}</div></div>
                <div className="row gap"><button className="btn primary" onClick={() => void nu.connect(entry.hid)}>{nu.status === "error" ? "Try again" : "Reconnect"}</button><button className="btn ghost" onClick={() => onRemove(entry.id)}>Remove</button></div>
              </>
            ) : (
              <div className="callout info"><i className="pulse" /><div><b>Connecting to {name}…</b> Handshaking over USB.</div></div>
            )}
          </div>
        </main>
        <Toasts toasts={nu.toasts} />
      </div>
    );
  }

  const cur = TABS.find((t) => t.id === tab)!;
  return (
    <div className="app">
      <aside className="rail">
        <div className="brand"><Mark /><div><b>Air Deck</b><span>{name} control</span></div></div>
        {menu}
        <nav>
          {TABS.map((t, i) => (
            <button key={t.id} className={`nav${t.id === tab ? " on" : ""}`} onClick={() => setTab(t.id)}>
              {t.icon}<span>{t.label}</span><kbd>{i + 1}</kbd>
            </button>
          ))}
        </nav>
        <div className="device">
          <dl>
            <div><dt>Session key</dt><dd className="mono">0x{nu.info?.k.toString(16)}</dd></div>
            <div><dt>Mode</dt><dd className="mode-pill"><i className={nu.activeMode === 1 ? "win" : "mac"} />{nu.activeMode === 1 ? "Windows" : "macOS"}</dd></div>
            <div><dt>Firmware</dt><dd className="mono">{nu.info ? parseFirmware(nu.info.firmware) : "—"}</dd></div>
            {nu.model.open && (
              <div title={nu.openFw ? `Built ${nu.openFw.build}` : "The firmware did not answer the diagnostics request"}>
                <dt>Build</dt><dd className="mono">{nu.openFw ? `r${nu.openFw.rev} · ${nu.openFw.build.slice(0, 6)}` : "unknown"}</dd>
              </div>
            )}
            {nu.model.topRight !== "none" && <div><dt>Top right</dt><dd>{nu.accessory === "button" ? "Button" : "Knob"}</dd></div>}
          </dl>
        </div>
      </aside>

      <main>
        {nu.model.open && (nu.openFw === null || nu.openFw.rev < 2) && (
          <div className="callout error stale"><IconAlert /><div><b>This keyboard is running older firmware.</b> Its top-right button is still on the old slot, so binding it from here is saved but does nothing. Flash the latest <span className="mono">air75v2.bin</span> and reconnect.</div></div>
        )}
        <header className="topbar">
          <div><h2>{cur.label}</h2><p>{cur.sub}</p></div>
          {DEMO && <span className="demo-flag">Demo mode</span>}
        </header>
        <div className="view" key={tab}>
          {tab === "keys" && <KeysPanel nu={nu} />}
          {tab === "advanced" && <AdvancedPanel nu={nu} />}
          {tab === "lighting" && <LightingPanel nu={nu} />}
          {tab === "macros" && <MacroPanel nu={nu} />}
          {tab === "console" && <Console nu={nu} />}
        </div>
      </main>
      {nu.status === "reconnecting" && (
        <div className="reconnect"><i className="pulse" /><div><b>Keyboard restarted</b><span>It does this after some writes. Reconnecting automatically…</span></div></div>
      )}
      <Toasts toasts={nu.toasts} />
    </div>
  );
}

export default function App() {
  const [entries, setEntries] = useState<Entry[]>([]);
  const [activeId, setActiveId] = useState<number | null>(null);
  const [summaries, setSummaries] = useState<Record<number, DeviceSummary>>({});
  const [picking, setPicking] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const nextId = useRef(1);

  const add = useCallback(async () => {
    setPicking(true); setError(null);
    try {
      const hid = await NuPhyDevice.pick();
      const existing = entries.find((e) => e.hid === hid);
      if (existing) { setActiveId(existing.id); return; }
      const id = nextId.current++;
      setEntries((l) => [...l, { id, hid }]);
      setActiveId(id);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally { setPicking(false); }
  }, [entries]);

  const remove = useCallback((id: number) => {
    setEntries((l) => { const next = l.filter((e) => e.id !== id); setActiveId((a) => (a === id ? next[next.length - 1]?.id ?? null : a)); return next; });
    setSummaries((s) => { const n = { ...s }; delete n[id]; return n; });
  }, []);
  const summary = useCallback((s: DeviceSummary) => setSummaries((m) => (JSON.stringify(m[s.id]) === JSON.stringify(s) ? m : { ...m, [s.id]: s })), []);

  if (!("hid" in navigator)) {
    return <div className="landing"><div className="landing-copy"><h1>WebHID isn't available.</h1><p className="lede">Open this page in Chrome, Edge or Brave over https or localhost.</p></div></div>;
  }
  if (!entries.length) return <Landing onConnect={() => void add()} busy={picking} error={error} demo={DEMO} />;

  const devices = entries.map((e) => summaries[e.id] ?? { id: e.id, name: e.hid.productName || "NuPhy keyboard", model: "unknown" as const, status: "connecting" as const });
  return (
    <>
      {entries.map((e) => (
        <Session key={e.id} entry={e} active={e.id === activeId} devices={devices} onSummary={summary} onSelect={setActiveId} onAdd={() => void add()} onRemove={remove} />
      ))}
    </>
  );
}
