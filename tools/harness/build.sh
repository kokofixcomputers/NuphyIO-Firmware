#!/bin/sh
# usage: ./build.sh [board]   -> libnph.dylib for that board's data (default air75v2)
cd "$(dirname "$0")"; B=${1:-air75v2}
python3 - <<'PY'
s=open('../../firmware/air75v2/src/core/nph_proto.c').read()
s=s.replace('#include "board_data.h"\n','#include "board_data.h"\nuint8_t sim_defkeys[NPH_KEYMAP_BYTES];\n',1)
s=s.replace('(uint8_t *)BD_DEFAULT_KEYMAP, NPH_KEYMAP_BYTES, false','sim_defkeys, NPH_KEYMAP_BYTES, false')
s=s.replace('    if (hooks) s->hooks = *hooks;','    if (hooks) s->hooks = *hooks;\n    memcpy(sim_defkeys, BD_DEFAULT_KEYMAP, sizeof sim_defkeys);',1)
open('nph_proto_h.c','w').write(s)
PY
cc -shared -fPIC -I ../../firmware/air75v2/src/boards/$B -I ../../firmware/air75v2/src/core -o libnph.dylib nph_shim.c nph_proto_h.c
