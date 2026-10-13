/**
 * Light state (cmd 0xD5 read / 0xD6 write), 17 bytes per mode; `handle` selects the mode (0 = Mac, 1 = Windows):
 *   back light (9 bytes @0):  [effect, brightness, speedRaw, b3, auto, b5, R, G, B]
 *   side light (8 bytes @9):  [effect, brightness, speedRaw, auto, b4, R, G, B]
 * speedRaw = 4 - gear. "auto" = 1 means effect-driven colours, 0 means use the RGB. Brightness 0 = off.
 * The official app writes the block, then the brightness byte alone (@1 back, @10 side).
 */
export interface EffectInfo { id: number; name: string; color: boolean; speed: boolean }

const e = (id: number, name: string, color: boolean, speed: boolean): EffectInfo => ({ id, name, color, speed });

export const BACK_EFFECTS: EffectInfo[] = [
  e(1, "Ray", true, true), e(2, "Stair", true, false), e(3, "Static", true, false), e(4, "Breath", true, true), e(5, "Flower", true, true),
  e(6, "Wave", true, true), e(7, "Ripple", true, true), e(8, "Spout", true, true), e(9, "Galaxy", true, true), e(10, "Rotation", true, true),
  e(11, "Ripple 2", true, true), e(12, "Point", true, true), e(13, "Grid", true, true), e(14, "Time", true, true), e(15, "Rain", true, true),
  e(16, "Ribbon", false, true), e(17, "Gaming", false, true), e(18, "Identify", false, true), e(19, "Windmill", false, true), e(20, "Diagonal", false, true),
];

export const SIDE_EFFECTS: EffectInfo[] = [
  e(0, "Time", true, true), e(1, "Neon", false, true), e(2, "Static", true, false), e(3, "Breathe", true, false), e(4, "Rhythm", true, false),
];

export const EFFECTS = BACK_EFFECTS.map((x) => x.name); // kept for older imports

export const LIGHT_BYTES = 17;
export const LED_COUNT = 104; // 84 key LEDs then 20 side LEDs
export const LED_BYTES = LED_COUNT * 3;
export const BACK_ADDR = 0, SIDE_ADDR = 9;

export interface BackLight { effect: number; brightness: number; gear: number; b3: number; auto: boolean; b5: number; r: number; g: number; b: number }
export interface SideLight { effect: number; brightness: number; gear: number; auto: boolean; b4: number; r: number; g: number; b: number }
export interface LightState { back: BackLight; side: SideLight }

const gear = (raw: number) => Math.max(0, Math.min(4, 4 - raw));
const raw = (g: number) => Math.max(0, Math.min(4, 4 - g));

export function parseLightState(d: ArrayLike<number>): LightState {
  return {
    back: { effect: d[0], brightness: d[1], gear: gear(d[2]), b3: d[3], auto: d[4] !== 0, b5: d[5], r: d[6], g: d[7], b: d[8] },
    side: { effect: d[9], brightness: d[10], gear: gear(d[11]), auto: d[12] !== 0, b4: d[13], r: d[14], g: d[15], b: d[16] },
  };
}

export const backBytes = (l: BackLight) => [l.effect, l.brightness, raw(l.gear), l.b3, l.auto ? 1 : 0, l.b5, l.r, l.g, l.b];
export const sideBytes = (l: SideLight) => [l.effect, l.brightness, raw(l.gear), l.auto ? 1 : 0, l.b4, l.r, l.g, l.b];
