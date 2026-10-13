import { useEffect, useState } from "react";
import { ledSlots } from "../layout";
import { IconAlert, IconPlug } from "./icons";
import KeyboardView from "./KeyboardView";

const slotIndex = new Map(ledSlots(60).map((s, i) => [s, i]));

export default function Landing({ onConnect, busy, error, demo }: { onConnect: () => void; busy: boolean; error: string | null; demo: boolean }) {
  const [t, setT] = useState(0);
  useEffect(() => { const id = setInterval(() => setT((x) => x + 1), 70); return () => clearInterval(id); }, []);

  const color = (slot: number) => {
    const i = slotIndex.get(slot) ?? 0;
    const h = ((slot & 15) * 15 + (slot >> 4) * 28 + t * 4) % 360;
    return `hsl(${h} 88% ${50 + 10 * Math.sin((t + i) / 9)}%)`;
  };

  return (
    <div className="landing">
      <div className="landing-copy">
        <span className="eyebrow"><span className="dot" /> WebHID · runs locally · no account</span>
        <h1>Configure<br />your NuPhy<br /><em>directly.</em></h1>
        <p className="lede">Remap every layer, shape the lighting with a live view of what the board is actually showing, and program macros — straight over USB, nothing leaves this machine.</p>
        <div className="cta">
          <button className="btn primary lg" disabled={busy} onClick={onConnect}>
            <IconPlug /> {busy ? "Handshaking…" : "Connect keyboard"}
          </button>
          {!demo && <a className="btn ghost lg" href="?demo">Explore demo</a>}
          {demo && <span className="demo-flag">Demo mode · simulated keyboard</span>}
        </div>
        {error && <div className="callout error"><IconAlert /><div><b>Couldn't connect.</b> {error}</div></div>}
      </div>

      <div className="landing-stage">
        <KeyboardView color={color} className="hero-kb" />
        <ol className="steps">
          <li><b>1</b><div><h4>Plug in over USB</h4><p>Use the cable, not Bluetooth or the 2.4&nbsp;GHz dongle.</p></div></li>
          <li><b>2</b><div><h4>Free the device</h4><p>Close nuphy.io or any other tab or app that has the keyboard open.</p></div></li>
          <li><b>3</b><div><h4>Authorize</h4><p>Choose your keyboard (Air75 V2 or V3) in the browser prompt. Chrome, Edge or Brave.</p></div></li>
        </ol>
      </div>
    </div>
  );
}
