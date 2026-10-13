import { ReactNode } from "react";

const base = { width: 18, height: 18, viewBox: "0 0 24 24", fill: "none", stroke: "currentColor", strokeWidth: 1.7, strokeLinecap: "round", strokeLinejoin: "round" } as const;
const I = (children: ReactNode) => () => <svg {...base} aria-hidden>{children}</svg>;

export const IconKeys = I(<><rect x="2.5" y="6" width="19" height="12" rx="2.5" /><path d="M6.5 10h.01M10 10h.01M13.5 10h.01M17 10h.01M7.5 14h9" /></>);
export const IconLight = I(<><circle cx="12" cy="12" r="3.5" /><path d="M12 3v2M12 19v2M3 12h2M19 12h2M5.6 5.6l1.4 1.4M17 17l1.4 1.4M5.6 18.4L7 17M17 7l1.4-1.4" /></>);
export const IconMacro = I(<><path d="M13 3L5 14h6l-1 7 8-11h-6l1-7z" /></>);
export const IconConsole = I(<><rect x="3" y="4.5" width="18" height="15" rx="2.5" /><path d="M7 10l3 2.5L7 15M12.5 15H17" /></>);
export const IconPlug = I(<><path d="M9 3v5M15 3v5M6.5 8h11v3a5.5 5.5 0 01-11 0V8zM12 16.5V21" /></>);
export const IconClose = I(<><path d="M6 6l12 12M18 6L6 18" /></>);
export const IconCheck = I(<><path d="M5 12.5l4.5 4.5L19 7.5" /></>);
export const IconAlert = I(<><path d="M12 8v5M12 16.5h.01" /><path d="M10.3 3.9L2.4 17.5a2 2 0 001.7 3h15.8a2 2 0 001.7-3L13.7 3.9a2 2 0 00-3.4 0z" /></>);
export const IconRec = I(<><circle cx="12" cy="12" r="5" fill="currentColor" stroke="none" /><circle cx="12" cy="12" r="9" /></>);
export const IconTrash = I(<><path d="M4 7h16M10 11v6M14 11v6M6 7l1 12a2 2 0 002 2h6a2 2 0 002-2l1-12M9 7V4h6v3" /></>);
export const IconCopy = I(<><rect x="8" y="8" width="12" height="12" rx="2" /><path d="M16 8V6a2 2 0 00-2-2H6a2 2 0 00-2 2v8a2 2 0 002 2h2" /></>);
export const IconAdvanced = I(<><path d="M4 7h10M18 7h2M4 17h2M10 17h10" /><circle cx="16" cy="7" r="2" /><circle cx="8" cy="17" r="2" /></>);
export const IconChevron = I(<><path d="M6 9.5l6 6 6-6" /></>);
export const IconPlus = I(<><path d="M12 5v14M5 12h14" /></>);
/** Air75 V3: a keyboard with a round knob in the top-right corner. */
export const IconBoardV3 = I(<><rect x="2.5" y="5.5" width="19" height="13" rx="2.5" /><circle cx="18.2" cy="9" r="1.7" /><path d="M6 9h.01M9 9h.01M12 9h.01M6.5 13h11M6 15.5h.01" /></>);
/** Air75 V2: a keyboard with a square button in the top-right corner. */
export const IconBoardV2 = I(<><rect x="2.5" y="5.5" width="19" height="13" rx="2.5" /><rect x="16.4" y="7.4" width="3.2" height="3.2" rx=".8" /><path d="M6 9h.01M9 9h.01M12 9h.01M6.5 13h11M6 15.5h.01" /></>);
/** Halo75 V2: a keyboard inside its light ring. */
export const IconBoardHalo = I(<><rect x="2" y="4" width="20" height="16" rx="4.5" strokeDasharray="1.6 2.2" /><rect x="6" y="8" width="12" height="8" rx="1.6" /><path d="M8.5 11h.01M11 11h.01M13.5 11h.01M16 11h.01M9 13.5h6" /></>);
export const IconBoardGeneric = IconKeys;
