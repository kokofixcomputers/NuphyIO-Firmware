import { Toast } from "../useNuphy";
import { IconAlert, IconCheck, IconPlug } from "./icons";

export default function Toasts({ toasts }: { toasts: Toast[] }) {
  return (
    <div className="toasts" aria-live="polite">
      {toasts.map((t) => (
        <div key={t.id} className={`toast ${t.kind}`}>
          {t.kind === "ok" ? <IconCheck /> : t.kind === "error" ? <IconAlert /> : <IconPlug />}
          <span>{t.msg}</span>
        </div>
      ))}
    </div>
  );
}
