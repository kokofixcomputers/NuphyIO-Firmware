import { useCallback, useEffect, useRef, useState } from "react";
import { ConnectInfo, NuPhyDevice, StoreStatus } from "./protocol/device";
import { LightState, parseLightState } from "./protocol/lighting";
import { Accessory } from "./layout";
import { KbModel, identify } from "./protocol/model";
import { keyAddr } from "./protocol/keymap";

export interface LogLine { t: number; dir: "tx" | "rx" | "info" | "warn"; msg: string }
export interface Toast { id: number; kind: "ok" | "error" | "info"; msg: string }
export type Status = "idle" | "connecting" | "connected" | "reconnecting" | "error";

export function useNuphy() {
  const devRef = useRef<NuPhyDevice | null>(null);
  const [status, setStatus] = useState<Status>("idle");
  const [error, setError] = useState<string | null>(null);
  const [info, setInfo] = useState<ConnectInfo | null>(null);
  const [log, setLog] = useState<LogLine[]>([]);
  const [keymap, setKeymap] = useState<Uint8Array | null>(null);
  const [light, setLight] = useState<LightState | null>(null);
  const [lightMode, setLightMode] = useState(0);
  const [activeMode, setActiveMode] = useState<0 | 1>(0);          // the keyboard's OS slide switch: 0 = Mac, 1 = Windows
  const activeRef = useRef<0 | 1>(0);
  const lightModeRef = useRef(0);
  lightModeRef.current = lightMode;
  const [macros, setMacros] = useState<Uint8Array | null>(null);
  const [leds, setLeds] = useState<Uint8Array | null>(null);
  const [openFw, setOpenFw] = useState<StoreStatus | null>(null);   // open-firmware diagnostics (revision, build time); null on a stock board
  const [productName, setProductName] = useState<string | undefined>(undefined);
  const [accessory, setAccessoryState] = useState<Accessory>("knob");
  const [polling, setPolling] = useState(false);
  const pollWanted = useRef(false);
  const [toasts, setToasts] = useState<Toast[]>([]);

  const push = useCallback((dir: LogLine["dir"], msg: string) => {
    setLog((l) => [...l.slice(-199), { t: Date.now(), dir, msg }]);
  }, []);

  const toast = useCallback((kind: Toast["kind"], msg: string) => {
    const id = Date.now() + Math.random();
    setToasts((t) => [...t.slice(-3), { id, kind, msg }]);
    setTimeout(() => setToasts((t) => t.filter((x) => x.id !== id)), 3800);
  }, []);

  const connect = useCallback(async (hid?: HIDDevice) => {
    setStatus("connecting");
    setError(null);
    const dev = new NuPhyDevice({
      log: push,
      mode: (m) => {
        activeRef.current = m; setActiveMode(m); setLightMode(m);
        toast("info", m === 1 ? "Keyboard switched to Windows mode" : "Keyboard switched to Mac mode");
      },
      reset: () => {
        toast("info", "Keyboard reset to factory settings");
        const d = devRef.current;
        if (!d) return;
        void (async () => {
          try {
            setKeymap(await d.readKeymap());
            setLight(await d.readLight(activeRef.current));
            setMacros(await d.readMacroRegion());
            setAccessoryState(await d.readAccessory().catch(() => "knob" as const));
          } catch (e) { push("warn", `re-read after reset failed: ${e instanceof Error ? e.message : String(e)}`); }
        })();
      },
      lightChanged: (bytes) => { if (lightModeRef.current === activeRef.current) setLight(parseLightState(bytes)); },
      disconnect: () => {
        setStatus("idle"); setInfo(null); setPolling(false);
        setKeymap(null); setLight(null); setMacros(null); setLeds(null);
        devRef.current = null;
      },
      lost: () => {
        setStatus("reconnecting");
        setPolling(false);
        void (async () => {
          try {
            const i = await dev.reconnect();
            setInfo(i);
            activeRef.current = i.base[0] === 1 ? 1 : 0; setActiveMode(activeRef.current);
            setLight(await dev.readLight(i.base[0] === 1 ? 1 : 0));
            setKeymap(await dev.readKeymap());
            setMacros(await dev.readMacroRegion());
            setAccessoryState(await dev.readAccessory().catch(() => "knob" as const));
            setStatus("connected");
            toast("info", "Keyboard restarted — reconnected");
            if (pollWanted.current) { dev.startPolling(setLeds, 50); setPolling(true); }
          } catch (e) {
            const msg = e instanceof Error ? e.message : String(e);
            setError(msg); setStatus("error"); push("warn", msg);
            setInfo(null); setKeymap(null); setLight(null); setMacros(null); setLeds(null);
            devRef.current = null;
          }
        })();
      },
    });
    devRef.current = dev;
    (window as unknown as { __nph?: NuPhyDevice }).__nph = dev;          // debug handle for scripted captures
    try {
      const i = hid ? await dev.connectTo(hid) : await dev.connect();
      setProductName(dev.productName);
      if (identify(dev.productName).open) setOpenFw(await dev.readStoreStatus()); else setOpenFw(null);
      setInfo(i);
      const mode = i.base[0] === 1 ? 1 : 0;
      activeRef.current = mode; setActiveMode(mode);
      setLightMode(mode);
      setLight(await dev.readLight(mode));
      setKeymap(await dev.readKeymap());
      setAccessoryState(await dev.readAccessory().catch(() => "knob" as const));   // button (V2) or knob (V3)
      setStatus("connected");
      push("info", "Read keymap + lighting state");
    } catch (e) {
      const msg = e instanceof Error ? e.message : String(e);
      setError(msg);
      setStatus("error");
      push("warn", `connect failed: ${msg}`);
      await dev.disconnect().catch(() => {});
    }
  }, [push, toast]);

  /** Update one keymap entry locally after a verified write. */
  const patchKey = useCallback((layer: number, slot: number, entry: number) => {
    setKeymap((m) => {
      if (!m) return m;
      const next = new Uint8Array(m);
      const a = keyAddr(layer, slot);
      next[a] = entry >> 8;
      next[a + 1] = entry & 0xff;
      return next;
    });
  }, []);

  const setAccessory = useCallback(async (a: Accessory) => {
    const d = devRef.current;
    if (!d) return;
    try { setAccessoryState(await d.writeAccessory(a)); toast("ok", a === "button" ? "Top-right set to a button" : "Top-right set to a knob"); }
    catch (e) { toast("error", e instanceof Error ? e.message : String(e)); }
  }, [toast]);

  const disconnect = useCallback(async () => { await devRef.current?.disconnect(); }, []);

  const startPolling = useCallback((rate = 50) => {
    pollWanted.current = true;
    devRef.current?.startPolling(setLeds, rate);
    setPolling(true);
  }, []);
  const stopPolling = useCallback(() => { pollWanted.current = false; devRef.current?.stopPolling(); setPolling(false); }, []);

  /** Run a device call; failures go to the log instead of throwing into React. */
  const run = useCallback(async <T,>(fn: (d: NuPhyDevice) => Promise<T>, okMsg?: string): Promise<T | undefined> => {
    const d = devRef.current;
    if (!d) return undefined;
    try {
      const r = await fn(d);
      if (okMsg) toast("ok", okMsg);
      return r;
    } catch (e) {
      const msg = e instanceof Error ? e.message : String(e);
      push("warn", msg);
      toast("error", msg);
      return undefined;
    }
  }, [push, toast]);

  useEffect(() => () => { void devRef.current?.disconnect(); }, []);

  return { status, error, info, log, keymap, setKeymap, patchKey, light, setLight, lightMode, setLightMode, activeMode, macros, setMacros, leds, note: push, productName, openFw, model: identify(productName) as KbModel, accessory, setAccessory, polling, toasts, toast, connect, disconnect, startPolling, stopPolling, run };
}
export type Nuphy = ReturnType<typeof useNuphy>;
