#!/bin/bash
# Run a Python tool script inside the editor (ProgrammaticToolset.execute_tool_script) and print the decoded result.
# The script defines run() and calls tools with execute_tool('<toolset>.<tool>', json_args); run()'s return value is printed.
# Выполнить Python-скрипт в редакторе (ProgrammaticToolset.execute_tool_script) и напечатать раскодированный результат.
# Скрипт объявляет run() и зовёт инструменты через execute_tool('<toolset>.<tool>', json_args); печатается то, что вернул run().
#
# usage: run.sh <script.py>
set -euo pipefail

if [ $# -lt 1 ]; then
	echo "usage: run.sh <script.py>" >&2
	exit 2
fi

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARGS="$(mktemp)"
trap 'rm -f "$ARGS"' EXIT

python - "$1" "$ARGS" <<'PY'
import json,sys
json.dump({'script':open(sys.argv[1],encoding='utf-8').read()},open(sys.argv[2],'w',encoding='utf-8'))
PY

"$HERE/mcp.sh" editor_toolset.toolsets.programmatic.ProgrammaticToolset execute_tool_script "$ARGS" < /dev/null | python -c "
import sys,json
t=sys.stdin.read(); t=t[t.find('{'):]
try:
    x=json.loads(t)['result']['content'][0]['text']
    try: x=json.loads(x)['returnValue']; x=json.dumps(json.loads(x),indent=1,ensure_ascii=False)
    except Exception: pass
    print(x)
except Exception as e: print('RAW',t[:2000])
"
