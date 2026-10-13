import { ComponentType, useEffect, useRef, useState } from "react";
import { ModelId } from "../protocol/model";
import { Status } from "../useNuphy";
import { IconBoardGeneric, IconBoardHalo, IconBoardV2, IconBoardV3, IconChevron, IconClose, IconPlus } from "./icons";

export interface DeviceSummary { id: number; name: string; model: ModelId; status: Status; detail?: string }

const ICONS: Record<ModelId, ComponentType> = { air75v2: IconBoardV2, halo75v2: IconBoardHalo, air75v3: IconBoardV3, unknown: IconBoardGeneric };
const STATUS_TEXT: Record<Status, string> = { idle: "Not connected", connecting: "Connecting…", connected: "Connected", reconnecting: "Reconnecting…", error: "Connection failed" };

/** Custom keyboard picker (not a native <select>, so it can carry icons, status and per-keyboard actions). */
export default function DeviceMenu({ devices, activeId, onSelect, onAdd, onRemove }: {
  devices: DeviceSummary[]; activeId: number; onSelect: (id: number) => void; onAdd: () => void; onRemove: (id: number) => void;
}) {
  const [open, setOpen] = useState(false);
  const root = useRef<HTMLDivElement>(null);
  const active = devices.find((d) => d.id === activeId) ?? devices[0];

  useEffect(() => {
    if (!open) return;
    const away = (e: MouseEvent) => { if (!root.current?.contains(e.target as Node)) setOpen(false); };
    const esc = (e: KeyboardEvent) => { if (e.key === "Escape") setOpen(false); };
    document.addEventListener("mousedown", away);
    document.addEventListener("keydown", esc);
    return () => { document.removeEventListener("mousedown", away); document.removeEventListener("keydown", esc); };
  }, [open]);

  if (!active) return null;
  const Icon = ICONS[active.model];
  return (
    <div className="dm" ref={root}>
      <button className={`dm-btn${open ? " open" : ""}`} aria-haspopup="listbox" aria-expanded={open} onClick={() => setOpen((o) => !o)}>
        <span className="dm-ico"><Icon /></span>
        <span className="dm-txt"><b>{active.name}</b><small><i className={`status ${active.status}`} />{STATUS_TEXT[active.status]}</small></span>
        <span className={`dm-chev${open ? " up" : ""}`}><IconChevron /></span>
      </button>
      {open && (
        <div className="dm-pop" role="listbox">
          {devices.map((d) => {
            const I = ICONS[d.model];
            return (
              <div key={d.id} role="option" aria-selected={d.id === activeId} tabIndex={0} className={`dm-item${d.id === activeId ? " on" : ""}`}
                onClick={() => { onSelect(d.id); setOpen(false); }}
                onKeyDown={(e) => { if (e.key === "Enter" || e.key === " ") { e.preventDefault(); onSelect(d.id); setOpen(false); } }}>
                <span className="dm-ico"><I /></span>
                <span className="dm-txt"><b>{d.name}</b><small><i className={`status ${d.status}`} />{d.detail ?? STATUS_TEXT[d.status]}</small></span>
                <button className="dm-x" title="Disconnect" aria-label={`Disconnect ${d.name}`} onClick={(e) => { e.stopPropagation(); setOpen(false); onRemove(d.id); }}><IconClose /></button>
              </div>
            );
          })}
          <button className="dm-add" onClick={() => { setOpen(false); onAdd(); }}><IconPlus /> Add keyboard</button>
        </div>
      )}
    </div>
  );
}
