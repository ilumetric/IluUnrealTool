#!/bin/bash
# Call one toolset tool of the Unreal MCP server (ModelContextProtocol plugin) and print the raw JSON-RPC answer.
# Вызвать один инструмент тулсета через MCP-сервер Unreal (плагин ModelContextProtocol) и напечатать сырой ответ JSON-RPC.
#
# usage: mcp.sh <toolset> <tool> <args.json>
#   mcp.sh IluUnrealToolAgent.IluMeshToolset InspectVertexColors args.json
#   args.json: {"mesh": {"refPath": "/Game/Props/SM_Box.SM_Box"}}
#
# ILU_MCP_URL — server address (default http://127.0.0.1:8000/mcp) / адрес сервера.
set -euo pipefail

if [ $# -lt 3 ]; then
	echo "usage: mcp.sh <toolset> <tool> <args.json>" >&2
	exit 2
fi

U="${ILU_MCP_URL:-http://127.0.0.1:8000/mcp}"
H=(-H "Content-Type: application/json" -H "Accept: application/json, text/event-stream")
REQ="$(mktemp)"
trap 'rm -f "$REQ"' EXIT

SID=$(curl -s -D - "${H[@]}" -X POST "$U" -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"ilu-cli","version":"1"}}}' -o /dev/null | grep -i '^mcp-session-id' | awk '{print $2}' | tr -d '\r')
if [ -z "$SID" ]; then
	echo "MCP server is not reachable at $U (is the editor running with ModelContextProtocol enabled?)" >&2
	exit 1
fi
curl -s "${H[@]}" -H "Mcp-Session-Id: $SID" -X POST "$U" -d '{"jsonrpc":"2.0","method":"notifications/initialized"}' >/dev/null

python -c "
import json,sys
a=json.load(open(sys.argv[3],encoding='utf-8'))
print(json.dumps({'jsonrpc':'2.0','id':2,'method':'tools/call','params':{'name':'call_tool','arguments':{'toolset_name':sys.argv[1],'tool_name':sys.argv[2],'arguments':a}}}))
" "$1" "$2" "$3" > "$REQ"
curl -s --max-time 590 "${H[@]}" -H "Mcp-Session-Id: $SID" -X POST "$U" --data-binary @"$REQ"
