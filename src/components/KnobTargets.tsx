import { describeEntry } from "../protocol/keymap";

/**
 * The Air75 V3 knob occupies three keymap slots: 96 = turn left, 97 = turn right (shown in that order
 * because the factory map is Vol- / Vol+), and 60 = push. They're assigned like any other key.
 */
export const KNOB_SLOTS = { left: 96, press: 60, right: 97 } as const;

const TARGETS = [
  { slot: KNOB_SLOTS.left, title: "Turn left", icon: "↺" },
  { slot: KNOB_SLOTS.press, title: "Press", icon: "●" },
  { slot: KNOB_SLOTS.right, title: "Turn right", icon: "↻" },
];

export default function KnobTargets({ sel, onSelect, entry }: { sel: number | null; onSelect: (slot: number) => void; entry: (slot: number) => number }) {
  return (
    <div className="knob">
      <div className="knob-dial" aria-hidden><i /></div>
      <div className="knob-targets">
        <span className="knob-title">Knob</span>
        <div className="knob-row">
          {TARGETS.map((t) => (
            <button key={t.slot} className={`knob-key${sel === t.slot ? " sel" : ""}`} title={`${t.title} · slot ${t.slot}`} onClick={() => onSelect(t.slot)}>
              <b>{describeEntry(entry(t.slot)) || "—"}</b>
              <small>{t.icon} {t.title}</small>
            </button>
          ))}
        </div>
      </div>
    </div>
  );
}
