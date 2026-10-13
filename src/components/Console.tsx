import { useEffect, useRef, useState } from "react";
import { Nuphy } from "../useNuphy";
import { IconCopy } from "./icons";
import { Card, Segmented } from "./ui";

type Filter = "all" | "tx" | "rx" | "warn";

export default function Console({ nu }: { nu: Nuphy }) {
  const end = useRef<HTMLDivElement>(null);
  const [filter, setFilter] = useState<Filter>("all");
  const lines = nu.log.filter((l) => filter === "all" || l.dir === filter || (filter === "warn" && l.dir === "warn"));

  useEffect(() => { end.current?.scrollIntoView({ block: "end" }); }, [lines.length]);

  return (
    <Card
      title="Wire log"
      aside={
        <div className="row gap">
          <Segmented size="sm" value={filter} onChange={setFilter} options={[{ value: "all", label: "All" }, { value: "tx", label: "TX" }, { value: "rx", label: "RX" }, { value: "warn", label: "Issues" }]} />
          <button className="btn ghost sm" onClick={() => void navigator.clipboard.writeText(nu.log.map((l) => `${new Date(l.t).toISOString()} ${l.dir} ${l.msg}`).join("\n"))}><IconCopy /> Copy</button>
        </div>
      }
    >
      <div className="console">
        {lines.length === 0 && <div className="empty-row">Nothing logged yet.</div>}
        {lines.map((l, i) => (
          <div key={i} className={`ln ${l.dir}`}>
            <span className="ts">{new Date(l.t).toLocaleTimeString([], { hour12: false })}</span>
            <b>{l.dir}</b>
            <span className="msg">{l.msg}</span>
          </div>
        ))}
        <div ref={end} />
      </div>
    </Card>
  );
}
