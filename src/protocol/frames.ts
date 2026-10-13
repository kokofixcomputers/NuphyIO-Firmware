/**
 * Wire format (verified against captures of the official app):
 *   [0]=0x55 out / 0xAA in, [1]=cmd, [2]=0, [3]=checksum (sum of bytes 4..63 mod 256),
 *   [4]=len^K, [5]=addrLo^K, [6]=addrHi^K, [7]=handle^K, [8..]=data^K, rest 0x00.
 * Every command is a read or write of a memory region; K is the session key (XOR cipher).
 */
export const FRAME_LEN = 64;
export const DATA_INDEX = 8;
export const MAX_DATA = FRAME_LEN - DATA_INDEX; // 56
export const OUT_FLAG = 0x55;
export const IN_FLAG = 0xaa;

export const CMD = {
  SET_SECRET: 0xee,
  GET_BASE: 0xa0,
  GET_FIRMWARE: 0xa1,
  GET_DEFAULT_KEYS: 0xb1,
  GET_KEYS: 0xb2,
  SET_KEYS: 0xb3,
  GET_TAPDANCE: 0xb8,
  SET_TAPDANCE: 0xb9,
  GET_TGL: 0xbb,
  SET_TGL: 0xbc,
  GET_MACRO: 0xc2,
  SET_MACRO: 0xc3,
  GET_LED_COLOR: 0xd2,
  GET_LIGHT: 0xd5,
  SET_LIGHT: 0xd6,
  SET_KEY_UPLOAD: 0xc1,
  GET_KBFUNC: 0xe1,
  SET_KBFUNC: 0xe2,
  GET_SLEEP: 0xf3,
  SET_SLEEP: 0xf5,
  GET_APPDEF: 0xfb,
  SET_APPDEF: 0xfc,
} as const;

export function checksum(f: Uint8Array): number {
  let s = 0;
  for (let i = 4; i < f.length; i++) s += f[i];
  return s & 0xff;
}

export function buildFrame(k: number, cmd: number, len: number, addr: number, handle = 0, data?: ArrayLike<number>): Uint8Array<ArrayBuffer> {
  const f = new Uint8Array(FRAME_LEN);
  f[0] = OUT_FLAG;
  f[1] = cmd;
  f[4] = len ^ k;
  f[5] = (addr & 0xff) ^ k;
  f[6] = ((addr >> 8) & 0xff) ^ k;
  f[7] = handle ^ k;
  if (data) for (let i = 0; i < data.length; i++) f[DATA_INDEX + i] = data[i] ^ k;
  f[3] = checksum(f);
  return f;
}

/** Handshake: 56 random payload bytes, K = payload[20] || 170 (as the official app does). */
export function buildHandshake(): { frame: Uint8Array<ArrayBuffer>; k: number } {
  const payload = new Uint8Array(MAX_DATA);
  crypto.getRandomValues(payload);
  const k = payload[20] || 170;
  const f = new Uint8Array(FRAME_LEN);
  f[0] = OUT_FLAG;
  f[1] = CMD.SET_SECRET;
  f.set(payload, DATA_INDEX);
  f[3] = checksum(f);
  return { frame: f, k };
}

export interface Parsed { cmd: number; len: number; addr: number; handle: number; data: Uint8Array; sumOk: boolean }

export function parseFrame(k: number, f: Uint8Array): Parsed {
  const len = f[4] ^ k;
  const data = new Uint8Array(Math.min(len, MAX_DATA));
  for (let i = 0; i < data.length; i++) data[i] = f[DATA_INDEX + i] ^ k;
  return {
    cmd: f[1],
    len,
    addr: (f[5] ^ k) | ((f[6] ^ k) << 8),
    handle: f[7] ^ k,
    data,
    sumOk: checksum(f) === f[3],
  };
}
