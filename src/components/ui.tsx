import { CSSProperties, ReactNode } from "react";

export function Segmented<T extends string | number>({ value, options, onChange, size = "md" }: {
  value: T; options: { value: T; label: ReactNode }[]; onChange: (v: T) => void; size?: "sm" | "md";
}) {
  return (
    <div className={`seg seg-${size}`} role="tablist">
      {options.map((o) => (
        <button key={String(o.value)} role="tab" aria-selected={o.value === value} className={o.value === value ? "on" : ""} onClick={() => onChange(o.value)}>
          {o.label}
        </button>
      ))}
    </div>
  );
}

export function Slider({ label, value, min, max, step = 1, unit = "", onChange }: {
  label: string; value: number; min: number; max: number; step?: number; unit?: string; onChange: (v: number) => void;
}) {
  const pct = ((value - min) / (max - min)) * 100;
  return (
    <label className="slider">
      <span className="slider-head"><span>{label}</span><output className="mono">{value}{unit}</output></span>
      <input type="range" min={min} max={max} step={step} value={value} style={{ "--p": `${pct}%` } as CSSProperties} onChange={(e) => onChange(+e.target.value)} />
    </label>
  );
}

export function Card({ title, aside, children, className = "" }: { title?: ReactNode; aside?: ReactNode; children: ReactNode; className?: string }) {
  return (
    <section className={`card ${className}`}>
      {(title || aside) && <header className="card-head"><h3>{title}</h3>{aside}</header>}
      {children}
    </section>
  );
}

export const Kbd = ({ children }: { children: ReactNode }) => <kbd className="kbd">{children}</kbd>;
