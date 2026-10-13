import { CSSProperties, ReactNode } from "react";
import { Accessory, ROWS, accessorySlot } from "../layout";

interface Props {
  label?: (slot: number) => string;
  icon?: (slot: number) => ReactNode | null;     // a glyph shown instead of the text label (media, brightness, macOS function keys, mouse...)
  color?: (slot: number) => string | undefined; // live LED colour
  selected?: number | null;
  onSelect?: (slot: number) => void;
  dim?: (slot: number) => boolean;
  className?: string;
  accessory?: Accessory;   // what fills the top-right corner: a knob (push = slot 60) or a button (slot 60 as well)
}

const UNITS = 16;

export default function KeyboardView({ label, color, selected, onSelect, dim, className = "", accessory, icon }: Props) {
  return (
    <div className={`kb ${className}`}>
      <div className="kb-case">
        {ROWS.map((row, r) => {
          const used = row.reduce((n, k) => n + k.w, 0);
          return (
            <div className="kb-row" key={r}>
              {row.map(({ slot, w }) => {
                const led = color?.(slot);
                const text = label?.(slot) ?? "";
                const ic = icon?.(slot) ?? null;
                return (
                  <button
                    key={slot}
                    className={`key${selected === slot ? " sel" : ""}${led ? " led" : ""}${dim?.(slot) ? " dim" : ""}${ic ? " ico" : text.length > 3 ? " long" : ""}`}
                    style={{ flex: w, ...(led ? ({ "--led": led } as CSSProperties) : {}) }}
                    title={text ? `${text} · slot ${slot}` : `slot ${slot}`}
                    onClick={() => onSelect?.(slot)}
                  >
                    {ic ?? <span>{text}</span>}
                  </button>
                );
              })}
              {used < UNITS && (r === 0 && accessory ? (() => {
                const slot = accessorySlot(accessory);
                const led = color?.(slot);
                const text = label?.(slot) ?? "";
                const ic = icon?.(slot) ?? null;
                return (
                  <button
                    key={`acc${slot}`}
                    className={`key acc ${accessory}${selected === slot ? " sel" : ""}${led ? " led" : ""}${ic ? " ico" : text.length > 3 ? " long" : ""}`}
                    style={{ flex: UNITS - used, ...(led ? ({ "--led": led } as CSSProperties) : {}) }}
                    title={accessory === "knob" ? `knob press · slot ${slot}` : `button · slot ${slot}`}
                    onClick={() => onSelect?.(slot)}
                  >
                    {ic ?? <span>{text}</span>}
                  </button>
                );
              })() : <span className="kb-spacer" style={{ flex: UNITS - used }} />)}
            </div>
          );
        })}
      </div>
    </div>
  );
}
