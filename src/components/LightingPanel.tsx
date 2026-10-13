import { useEffect, useMemo, useRef, useState } from "react";
import { accessorySlot, ledSlots } from "../layout";
import { describeEntry, entryAt } from "../protocol/keymap";
import { BACK_EFFECTS, BackLight, EffectInfo, SIDE_EFFECTS, SideLight } from "../protocol/lighting";
import { Nuphy } from "../useNuphy";
import KeyboardView from "./KeyboardView";
import { Card, Segmented, Slider } from "./ui";

const hex = (n: number) => n.toString(16).padStart(2, "0");
const PRESETS = ["#ff5d5d", "#ff9f43", "#ffd93d", "#8bf05e", "#2ee6a6", "#3ec1ff", "#7a8bff", "#c77dff", "#ff6bb5", "#ffffff"];

type Common = { effect: number; brightness: number; gear: number; auto: boolean; r: number; g: number; b: number };

function LightCard<L extends Common>({ title, subtitle, light, effects, onChange }: {
  title: string; subtitle: string; light: L; effects: EffectInfo[]; onChange: (patch: Partial<L>) => void;
}) {
  const lastOn = useRef(light.brightness || 60);
  if (light.brightness > 0) lastOn.current = light.brightness;
  const info = effects.find((x) => x.id === light.effect);
  const color = `#${hex(light.r)}${hex(light.g)}${hex(light.b)}`;
  const setHex = (h: string) => { const v = parseInt(h.slice(1), 16); onChange({ auto: false, r: v >> 16, g: (v >> 8) & 255, b: v & 255 } as Partial<L>); };
  const on = light.brightness > 0;

  return (
    <Card
      title={title}
      aside={
        <button role="switch" aria-checked={on} aria-label={`${title} power`} className={`switch${on ? " on" : ""}`} onClick={() => onChange({ brightness: on ? 0 : lastOn.current } as Partial<L>)}><i /></button>
      }
    >
      <p className="hint tight">{subtitle}</p>
      <div className={`light-body${on ? "" : " off"}`}>
        <div className="effects">
          {effects.map((x) => (
            <button key={x.id} className={`effect${light.effect === x.id ? " on" : ""}`} onClick={() => onChange({ effect: x.id } as Partial<L>)}>{x.name}</button>
          ))}
        </div>
        <div className="stack tight">
          <Slider label="Brightness" value={light.brightness} min={0} max={100} unit="%" onChange={(v) => onChange({ brightness: v } as Partial<L>)} />
          {info?.speed !== false && <Slider label="Speed" value={light.gear} min={0} max={4} onChange={(v) => onChange({ gear: v } as Partial<L>)} />}
          {info?.color && (
            <div className="field">
              <span className="row gap between">
                <span>Custom colour <span className="mono faint">{light.auto ? "automatic" : color}</span></span>
                <button role="switch" aria-checked={!light.auto} aria-label="Use custom colour" className={`switch sm${!light.auto ? " on" : ""}`} onClick={() => onChange({ auto: !light.auto } as Partial<L>)}><i /></button>
              </span>
              <div className={`swatches${light.auto ? " dim" : ""}`}>
                {PRESETS.map((c) => <button key={c} className={`sw${!light.auto && c === color ? " on" : ""}`} style={{ background: c }} aria-label={c} onClick={() => setHex(c)} />)}
                <label className="sw picker" title="Pick any colour"><input type="color" value={color} onChange={(e) => setHex(e.target.value)} /></label>
              </div>
            </div>
          )}
        </div>
      </div>
    </Card>
  );
}

export default function LightingPanel({ nu }: { nu: Nuphy }) {
  const [rate, setRate] = useState(50);
  const dirty = useRef<{ back: boolean; side: boolean }>({ back: false, side: false });
  const timers = useRef<{ back?: number; side?: number }>({});
  const light = nu.light;
  const mode = nu.lightMode;
  const activeMode = nu.activeMode;

  // Debounced writes, one channel per light so they stay independent.
  useEffect(() => {
    if (!dirty.current.back || !light) return;
    window.clearTimeout(timers.current.back);
    timers.current.back = window.setTimeout(() => { dirty.current.back = false; void nu.run((d) => d.writeBack(mode, light.back)); }, 80);
    return () => { window.clearTimeout(timers.current.back); };
  }, [light?.back]); // eslint-disable-line react-hooks/exhaustive-deps
  useEffect(() => {
    if (!dirty.current.side || !light) return;
    window.clearTimeout(timers.current.side);
    timers.current.side = window.setTimeout(() => { dirty.current.side = false; void nu.run((d) => d.writeSide(mode, light.side)); }, 80);
    return () => { window.clearTimeout(timers.current.side); };
  }, [light?.side]); // eslint-disable-line react-hooks/exhaustive-deps

  // Live preview runs while this tab is open.
  useEffect(() => {
    nu.startPolling(rate);
    return () => { nu.stopPolling(); };
  }, [rate]); // eslint-disable-line react-hooks/exhaustive-deps

  const slotToLed = useMemo(() => new Map(ledSlots(accessorySlot(nu.accessory)).map((s, i) => [s, i])), [nu.accessory]);
  const colorFor = (slot: number) => {
    const i = slotToLed.get(slot);
    if (!nu.leds || i === undefined) return undefined;
    return `rgb(${nu.leds[i * 3]},${nu.leds[i * 3 + 1]},${nu.leds[i * 3 + 2]})`;
  };
  // The 20 side LEDs follow the 84 key LEDs.
  const sideSwatch = useMemo(() => {
    if (!nu.leds) return [] as string[];
    return Array.from({ length: 20 }, (_, i) => { const o = (84 + i) * 3; return `rgb(${nu.leds![o]},${nu.leds![o + 1]},${nu.leds![o + 2]})`; });
  }, [nu.leds]);

  const switchMode = async (m: number) => {
    nu.setLightMode(m);
    const s = await nu.run((d) => d.readLight(m));
    if (s) nu.setLight(s);
  };

  if (!light) return null;
  const patchBack = (p: Partial<BackLight>) => { dirty.current.back = true; nu.setLight({ ...light, back: { ...light.back, ...p } }); };
  const patchSide = (p: Partial<SideLight>) => { dirty.current.side = true; nu.setLight({ ...light, side: { ...light.side, ...p } }); };

  return (
    <div className="stack">
      <Card
        title="Live preview"
        aside={<span className={`live ${nu.polling && nu.leds ? "on" : ""}`}><i /> {nu.polling && nu.leds ? "Live from keyboard" : "Connecting…"}</span>}
      >
        <KeyboardView color={colorFor} label={(s) => (nu.keymap ? describeEntry(entryAt(nu.keymap, 0, s)) : "")} className="preview-kb" />
        {sideSwatch.length > 0 && (
          <div className="side-strip" title="The 20 side LEDs, in order">
            {sideSwatch.map((c, i) => <i key={i} style={{ background: c, boxShadow: `0 0 12px -2px ${c}` }} />)}
          </div>
        )}
        <p className="hint">Real LED values from the keyboard — the strip under the board is its 20 side LEDs. The preview shows the currently active mode.</p>
      </Card>

      <div className="row gap wrap between">
        <div className="row gap">
          <span className="hint nm">Profile</span>
          <Segmented value={mode} onChange={(v) => void switchMode(v)} options={[{ value: 0, label: `Mac${activeMode === 0 ? " · active" : ""}` }, { value: 1, label: `Windows${activeMode === 1 ? " · active" : ""}` }]} />
        </div>
        <details className="adv inline">
          <summary>Advanced</summary>
          <div className="row gap wrap">
            <label className="field inline">Poll <input type="number" min={10} value={rate} onChange={(e) => setRate(Math.max(10, +e.target.value))} /> ms</label>
          </div>
        </details>
      </div>

      <div className="grid lighting-grid">
        <LightCard title="Back light" subtitle="The per-key RGB under the keycaps." light={light.back} effects={BACK_EFFECTS} onChange={patchBack} />
        <LightCard title="Side light" subtitle="The LED strips along the case edges — independent of the back light." light={light.side} effects={SIDE_EFFECTS} onChange={patchSide} />
      </div>
    </div>
  );
}
