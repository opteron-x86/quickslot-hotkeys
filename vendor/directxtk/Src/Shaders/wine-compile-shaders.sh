#!/bin/sh
set -u
CompileShadersOutput="$1"
FxcTool="$2"
shift 2
find "$CompileShadersOutput" -type f -name '*.inc' -delete 2>/dev/null || true
"/usr/bin/cmake" -E env CompileShadersOutput="$CompileShadersOutput" WINEDEBUG=-all LegacyShaderCompiler="$FxcTool" wine cmd /c CompileShaders.cmd "$@" > "$CompileShadersOutput/compileshaders.log" 2>&1
if grep -q "Got an error" "$CompileShadersOutput/compileshaders.log"; then
    echo "fxc2 reported shader compilation error(s); see $CompileShadersOutput/compileshaders.log" >&2
    exit 1
fi
# Derived, not hardcoded, so a DirectXTK version bump can't silently
# invalidate it. grep -c exits 1 on no match, hence `|| true`.
expected_count=$(grep -c '^"' "$CompileShadersOutput/compileshaders.log" 2>/dev/null || true)
[ -n "$expected_count" ] || expected_count=0
inc_count=$(find "$CompileShadersOutput" -type f -name '*.inc' 2>/dev/null | wc -l)
if [ "$expected_count" -eq 0 ] || [ "$inc_count" -ne "$expected_count" ]; then
    echo "Shader compilation produced $inc_count .inc file(s), expected $expected_count; see $CompileShadersOutput/compileshaders.log" >&2
    echo "--- diagnostics ---" >&2
    ls -la "$FxcTool" >&2 2>&1 || echo "(no fxc2 binary at that path)" >&2
    command -v file >/dev/null 2>&1 && file "$FxcTool" >&2 2>&1 || true
    wine --version >&2 2>&1 || true
    echo "--- direct wine invocation, bypassing cmd.exe ---" >&2
    WINEDEBUG=-all wine "$FxcTool" >&2 2>&1 || echo "(direct wine invocation also failed)" >&2
    exit 1
fi
exit 0
