import { ReactNode } from "react";

/**
 * Glyphs for the keys that have no letter on them: media, brightness, the macOS function row (Mission Control, Spotlight, dictation,
 * Do Not Disturb, screenshots), Windows shortcuts, radio links and mouse buttons. Plain line icons drawn for Air Deck (not copied from
 * anywhere), in the spirit of the symbols printed on the keycaps. `keyIcon(entry)` returns null for keys that read better as text.
 */
const S = ({ children }: { children: ReactNode }) => (
  <svg className="ki" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round" aria-hidden>{children}</svg>
);
const icon = (children: ReactNode) => <S>{children}</S>;

const speaker = <path d="M4 9.5h3.5L12 5.5v13l-4.5-4H4z" />;
const sun = (r: number, ray: number) => (
  <>
    <circle cx="12" cy="12" r={r} />
    {Array.from({ length: 8 }, (_, i) => {
      const a = (i * Math.PI) / 4, r0 = r + 2.6, r1 = r0 + ray;
      return <path key={i} d={`M${(12 + Math.cos(a) * r0).toFixed(2)} ${(12 + Math.sin(a) * r0).toFixed(2)}L${(12 + Math.cos(a) * r1).toFixed(2)} ${(12 + Math.sin(a) * r1).toFixed(2)}`} />;
    })}
  </>
);
const moon = <path d="M20 14.5A8 8 0 019.5 4a8 8 0 1010.5 10.5z" />;
const mic = (<><rect x="9" y="3" width="6" height="11" rx="3" /><path d="M5.5 11a6.5 6.5 0 0013 0M12 17.5V21" /></>);
const monitor = (<><rect x="3" y="4" width="18" height="12" rx="2" /><path d="M8 20h8M12 16v4" /></>);
const lock = (<><rect x="5" y="11" width="14" height="9" rx="2" /><path d="M8 11V8a4 4 0 018 0v3" /></>);
const globe = (<><circle cx="12" cy="12" r="8" /><path d="M4 12h16M12 4c3 3.5 3 12.5 0 16M12 4c-3 3.5-3 12.5 0 16" /></>);
const tri = (flip: boolean) => <path d={flip ? "M19 5l-9 7 9 7z" : "M5 5l9 7-9 7z"} />;

/** a mouse with one region filled: l, r, m, or the side buttons */
const mouse = (part: "l" | "r" | "m" | "back" | "fwd") => (
  <>
    <rect x="6.5" y="3" width="11" height="18" rx="5.5" />
    <path d="M12 3v7M6.5 10h11" />
    {part === "l" && <path d="M6.5 10V8.5A5.5 5.5 0 0112 3v7z" fill="currentColor" stroke="none" />}
    {part === "r" && <path d="M17.5 10V8.5A5.5 5.5 0 0012 3v7z" fill="currentColor" stroke="none" />}
    {part === "m" && <rect x="10.8" y="4.6" width="2.4" height="4" rx="1.2" fill="currentColor" stroke="none" />}
    {(part === "back" || part === "fwd") && <rect x={part === "back" ? 3.4 : 18.2} y="9" width="2.4" height="4.5" rx="1" fill="currentColor" stroke="none" />}
  </>
);
const bt = (n: string) => (
  <>
    <path d="M6.5 8l9 8-4.5 4V4l4.5 4-9 8" />
    <text x="19" y="21" fontSize="9" fontWeight="700" fill="currentColor" stroke="none" textAnchor="middle" fontFamily="system-ui, sans-serif">{n}</text>
  </>
);

const ICONS: Record<number, ReactNode> = {
  // consumer / system usages (QMK numbering)
  0xa5: icon(<><path d="M12 3v9" /><path d="M6.5 6.5a8 8 0 1011 0" /></>),                                  // power
  0xa6: icon(<>{moon}<path d="M14 3h4l-4 4h4" /></>),                                                          // sleep
  0xa7: icon(sun(3.2, 2.2)),                                                                                   // wake
  0xa8: icon(<>{speaker}<path d="M16 9.5l5 5M21 9.5l-5 5" /></>),                                              // mute
  0xa9: icon(<>{speaker}<path d="M15.5 9.5a4 4 0 010 5M18.2 7a7.5 7.5 0 010 10" /></>),                       // volume up
  0xaa: icon(<>{speaker}<path d="M15.5 9.5a4 4 0 010 5" /></>),                                                // volume down
  0xab: icon(<><path d="M4 5l8 7-8 7zM13 5l8 7-8 7z" /></>),                                                   // next
  0xac: icon(<><path d="M20 5l-8 7 8 7zM11 5l-8 7 8 7z" /></>),                                                // previous
  0xad: icon(<rect x="6" y="6" width="12" height="12" rx="1.5" />),                                           // stop
  0xae: icon(<>{tri(false)}<path d="M17 5v14M20.5 5v14" /></>),                                                // play / pause
  0xb0: icon(<><path d="M12 5l8 9H4z" /><path d="M5 18.5h14" /></>),                                          // eject
  0xb1: icon(<><rect x="3" y="5" width="18" height="14" rx="2" /><path d="M3.5 7l8.5 6 8.5-6" /></>),         // mail
  0xb2: icon(<><rect x="5" y="3" width="14" height="18" rx="2" /><rect x="8" y="6" width="8" height="3" /><path d="M8.5 13h.01M12 13h.01M15.5 13h.01M8.5 17h.01M12 17h.01M15.5 17h.01" /></>), // calculator
  0xb3: icon(monitor),                                                                                         // my computer
  0xb4: icon(<><circle cx="10.5" cy="10.5" r="6" /><path d="M15 15l5 5" /></>),                                // search
  0xb5: icon(<path d="M4 11l8-7 8 7v8a1 1 0 01-1 1h-4v-6H9v6H5a1 1 0 01-1-1z" />),                            // web home
  0xb6: icon(<path d="M19 12H6m5-6l-6 6 6 6" />),                                                              // web back
  0xb7: icon(<path d="M5 12h13m-5-6l6 6-6 6" />),                                                              // web forward
  0xb8: icon(<><circle cx="12" cy="12" r="8" /><path d="M9 9l6 6M15 9l-6 6" /></>),                           // web stop
  0xb9: icon(<path d="M20 12a8 8 0 11-2.5-5.8M20 4v5h-5" />),                                                  // refresh
  0xba: icon(<path d="M12 3.5l2.6 5.4 5.9.8-4.3 4.1 1 5.9L12 17l-5.2 2.7 1-5.9L3.5 9.7l5.9-.8z" />),          // favourites
  0xbb: icon(<><path d="M4 6l6 6-6 6M12 6l6 6-6 6" /></>),                                                     // fast forward
  0xbc: icon(<><path d="M20 6l-6 6 6 6M12 6l-6 6 6 6" /></>),                                                  // rewind
  0xbd: icon(sun(3.6, 2.4)),                                                                                   // brightness up
  0xbe: icon(sun(2, 1.2)),                                                                                     // brightness down
  // F14 / F15 are the Mac brightness keys
  0x69: icon(sun(2, 1.2)),
  0x6a: icon(sun(3.6, 2.4)),
  // NuPhy functions
  0x7e02: icon(<><rect x="8" y="3" width="8" height="9" rx="1.5" /><path d="M8 12h8v4a4 4 0 01-8 0zM11 6.5h.01M13 6.5h.01" /></>),   // 2.4 GHz dongle
  0x7e03: icon(bt("1")), 0x7e04: icon(bt("2")), 0x7e05: icon(bt("3")),                                       // Bluetooth 1-3
  0x7e06: icon(<><rect x="3" y="4" width="18" height="6" rx="1.5" /><rect x="3" y="13" width="8" height="7" rx="1.5" /><rect x="13" y="13" width="8" height="7" rx="1.5" /></>),   // Mission Control
  0x7e07: icon(<><circle cx="10.5" cy="10.5" r="6" /><path d="M15 15l5 5" /></>),                              // Spotlight
  0x7e08: icon(mic),                                                                                           // dictation
  0x7e09: icon(<>{[4, 10, 16].flatMap((x) => [4, 10, 16].map((y) => <rect key={`${x}${y}`} x={x} y={y} width="4" height="4" rx="1" />))}</>),   // Launchpad
  0x7e0a: icon(<><path d="M4 9V6a2 2 0 012-2h3M15 4h3a2 2 0 012 2v3M20 15v3a2 2 0 01-2 2h-3M9 20H6a2 2 0 01-2-2v-3" /><circle cx="12" cy="12" r="2.2" /></>),   // screenshot
  0x7e0b: icon(<><rect x="4" y="4" width="16" height="16" rx="2" strokeDasharray="3 3" /><path d="M12 9v6M9 12h6" /></>),                       // screenshot area
  0x7e16: icon(moon),                                                                                          // Do Not Disturb
  0x7e3e: icon(<><rect x="3" y="5" width="18" height="14" rx="2.5" /><circle cx="12" cy="12" r="3" fill="currentColor" /></>),                   // screenshot / record
  0x7e3f: icon(lock), 0x7e40: icon(lock),                                                                      // lock screen
  0x7e41: icon(<><circle cx="10.5" cy="10.5" r="6" /><path d="M15 15l5 5M8.5 10.5h4" /></>),                   // Finder search
  0x7e42: icon(<><circle cx="12" cy="12" r="8" /><path d="M9 9l6 6M15 9l-6 6" /></>),                          // force quit
  0x7e43: icon(<><circle cx="12" cy="12" r="8" /><path d="M8.5 14a4.5 4.5 0 007 0M9 10h.01M15 10h.01" /></>),   // emoji
  0x7e44: icon(monitor),                                                                                       // back to desktop
  0x7e45: icon(<path d="M4 19V9M10 19V5M16 19v-7M21 19v-4" />),                                               // task manager
  0x7e47: icon(<><rect x="3" y="6" width="18" height="12" rx="2" /><path d="M16 12H8m3-3l-3 3 3 3" /></>),    // desktop left
  0x7e48: icon(<><rect x="3" y="6" width="18" height="12" rx="2" /><path d="M8 12h8m-3-3l3 3-3 3" /></>),     // desktop right
  0x7e49: icon(<><rect x="3" y="6" width="18" height="12" rx="2" /><path d="M16 12H8m3-3l-3 3 3 3" /></>),
  0x7e4a: icon(<><rect x="3" y="6" width="18" height="12" rx="2" /><path d="M8 12h8m-3-3l3 3-3 3" /></>),
  0x7e4f: icon(<><path d="M4 8h10M18 8h2M4 16h2M10 16h10" /><circle cx="16" cy="8" r="2" /><circle cx="8" cy="16" r="2" /></>),   // control centre
  0x7e52: icon(<><rect x="3" y="5" width="18" height="14" rx="2" /><path d="M3 15h18" /></>),                  // show desktop
  0x7e53: icon(globe),                                                                                         // language
  0x7e54: icon(mic),                                                                                           // Windows dictation
  0x7e15: icon(<><rect x="3" y="8" width="16" height="9" rx="2" /><path d="M21 11v3M6.5 11v3M10 11v3" /></>),   // battery
  0x7e3c: icon(<><rect x="3" y="9" width="18" height="6" rx="3" /><path d="M8 12h.01M12 12h.01M16 12h.01" /></>), // side light
  0x7827: icon(sun(3.6, 2.4)), 0x7828: icon(sun(2, 1.2)),                                                      // back-light brightness
  0x7e0d: icon(sun(3.6, 2.4)), 0x7e0e: icon(sun(2, 1.2)),                                                      // side-light brightness
  0x7e14: icon(<>{moon}<path d="M14 3h4l-4 4h4" /></>),                                                        // auto sleep
  // lighting controls
  0x7821: icon(<><path d="M12 3l1.8 5.2L19 10l-5.2 1.8L12 17l-1.8-5.2L5 10l5.2-1.8z" /><path d="M18.5 15.5l.8 2.2 2.2.8-2.2.8-.8 2.2-.8-2.2-2.2-.8 2.2-.8z" /></>),   // next effect
  0x7e0f: icon(<><path d="M12 3l1.8 5.2L19 10l-5.2 1.8L12 17l-1.8-5.2L5 10l5.2-1.8z" /><path d="M18.5 15.5l.8 2.2 2.2.8-2.2.8-.8 2.2-.8-2.2-2.2-.8 2.2-.8z" /></>),
  0x7829: icon(<path d="M6 13l6-6 6 6M6 19l6-6 6 6" />), 0x782a: icon(<path d="M6 5l6 6 6-6M6 11l6 6 6-6" />),            // speed up / down
  0x7e11: icon(<path d="M6 13l6-6 6 6M6 19l6-6 6 6" />), 0x7e12: icon(<path d="M6 5l6 6 6-6M6 11l6 6 6-6" />),
  0x7823: icon(<><circle cx="12" cy="12" r="8.5" /><circle cx="8.5" cy="10" r="1.2" fill="currentColor" /><circle cx="12" cy="7.8" r="1.2" fill="currentColor" /><circle cx="15.5" cy="10" r="1.2" fill="currentColor" /><circle cx="15.2" cy="14.6" r="1.2" fill="currentColor" /></>),   // hue (palette)
  0x7e10: icon(<><circle cx="12" cy="12" r="8.5" /><circle cx="8.5" cy="10" r="1.2" fill="currentColor" /><circle cx="12" cy="7.8" r="1.2" fill="currentColor" /><circle cx="15.5" cy="10" r="1.2" fill="currentColor" /><circle cx="15.2" cy="14.6" r="1.2" fill="currentColor" /></>),
  0x7e13: icon(<path d="M4 12a8 8 0 102.5-5.8M4 4v5h5" />),                                                    // reset
  0x7e19: icon(<><circle cx="9" cy="9" r="4.5" /><circle cx="15" cy="9" r="4.5" /><circle cx="12" cy="14.5" r="4.5" /></>),   // RGB test
  0x7e3b: icon(<><rect x="3.5" y="5" width="17" height="14" rx="3" /><text x="12" y="15.8" fontSize="9.5" fontWeight="700" fill="currentColor" stroke="none" textAnchor="middle" fontFamily="system-ui, sans-serif">A</text></>),   // caps indicator
  0x7e3d: icon(<path d="M4 8h13m-3-3l3 3-3 3M20 16H7m3-3l-3 3 3 3" />),                                       // F-row reverse
  // mouse
  0xd1: icon(mouse("l")), 0xd2: icon(mouse("r")), 0xd3: icon(mouse("m")), 0xd4: icon(mouse("back")), 0xd5: icon(mouse("fwd")),
  0xd9: icon(<path d="M12 19V6M6.5 11.5L12 6l5.5 5.5" />), 0xda: icon(<path d="M12 5v13M6.5 12.5L12 18l5.5-5.5" />),
  0xdb: icon(<path d="M19 12H6M11.5 6.5L6 12l5.5 5.5" />), 0xdc: icon(<path d="M5 12h13M12.5 6.5L18 12l-5.5 5.5" />),
};

export function keyIcon(entry: number): ReactNode | null { return ICONS[entry] ?? null; }
export const hasKeyIcon = (entry: number) => entry in ICONS;
