#!/usr/bin/env bash
# Builds and runs every engine-independent native test with g++ (Linux/WSL/CI).
# Scope: pure C++ cores and Python tools only. This does NOT build Unreal,
# does NOT run MSVC /W4 /WX, and does NOT touch a physical controller, another
# PC, a network or a packaged game. Windows release checks keep using the
# per-test PowerShell runners (Tests/run-*-tests.ps1).
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$ROOT/Source/ImperialWingsSimulator"
CXX="${CXX:-g++}"
STAMP="$(date -u +%Y%m%d-%H%M%S)"
OUT="${NATIVE_TEST_OUT:-$ROOT/Saved/Verification/native-core-linux-$STAMP}"
mkdir -p "$OUT"

# Existing tests predate the strict flag set and are built as before (warnings allowed).
LEGACY_FLAGS=(-std=c++17 -O2 -Wall -Wextra)
# New 2026-10-31 cores are held to an approximation of MSVC /W4 /WX.
STRICT_FLAGS=(-std=c++17 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror)

LEGACY_TESTS=(ai_attacker_tests ai_recovery_tests ai_training_tests first_flight_guide_tests
  guidance_request_tests hud_readability_tests manual_throttle_tests physics_models_tests
  pilot_response_tests radio_tests soccer_rules_tests)
STRICT_TESTS=(topgun_tests match_cues_tests practice_gravity_tests los_auto_framing_tests
  pilot_assist_tests match_officiating_tests ai_obstacle_planner_tests net_session_core_tests
  training_goals_tests)
# Headers that Unreal will include must not need exceptions or RTTI.
STRICT_HEADERS=(TopgunCourse.h TopgunRecords.h MatchPresentationCues.h PracticeGravity.h
  LosAutoFraming.h PilotAssistDiagnostics.h MatchOfficiating.h AIObstaclePlanner.h
  NetSessionCore.h TrainingGoals.h)
PY_TESTS=(test-diagnostic-bundle.py test-check-localization.py)

# Extra engine-independent sources a test needs are read from its PowerShell runner
# so the Windows and Linux runners cannot silently diverge.
extra_sources() {
  local test="$1" ps1 name
  for ps1 in "$ROOT"/Tests/run-*.ps1; do
    grep -q "$test.cpp" "$ps1" || continue
    for name in $(grep -oE "[A-Za-z]+\.cpp" "$ps1" | sort -u); do
      [ "$name" = "$test.cpp" ] && continue
      [ -f "$SRC/$name" ] && printf '%s\n' "$SRC/$name"
    done
    return
  done
}

pass=0; fail=0; rows=()
record() { rows+=("$1|$2|$3"); if [ "$2" = PASS ]; then pass=$((pass+1)); else fail=$((fail+1)); fi; printf '%-34s %-5s %s\n' "$1" "$2" "$3"; }

run_cpp() {
  local test="$1"; shift
  local flags=("$@") srcfile="$ROOT/Tests/$test.cpp" exe="$OUT/$test" extras=() objects=() extra obj
  if [ ! -f "$srcfile" ]; then record "$test" FAIL "missing $srcfile"; return; fi
  mapfile -t extras < <(extra_sources "$test")
  : >"$OUT/$test.compile.log"
  if ! "$CXX" "${flags[@]}" -I"$SRC" -c "$srcfile" -o "$OUT/$test.o" >>"$OUT/$test.compile.log" 2>&1; then
    record "$test" FAIL "compile failed (see $test.compile.log)"; return
  fi
  objects=("$OUT/$test.o")
  # Pre-existing engine-independent sources (for example DroneSoccerRules.cpp) keep
  # their original flag set; only the new test translation unit is held to -Werror.
  for extra in "${extras[@]}"; do
    obj="$OUT/$test.$(basename "${extra%.cpp}").o"
    if ! "$CXX" "${LEGACY_FLAGS[@]}" -I"$SRC" -c "$extra" -o "$obj" >>"$OUT/$test.compile.log" 2>&1; then
      record "$test" FAIL "compile failed (see $test.compile.log)"; return
    fi
    objects+=("$obj")
  done
  if ! "$CXX" "${objects[@]}" -o "$exe" >>"$OUT/$test.compile.log" 2>&1; then
    record "$test" FAIL "compile failed (see $test.compile.log)"; return
  fi
  if "$exe" >"$OUT/$test.log" 2>&1; then record "$test" PASS "$(tail -n 1 "$OUT/$test.log")"
  else record "$test" FAIL "exit $? : $(tail -n 1 "$OUT/$test.log")"; fi
}

echo "== legacy native tests (g++ ${LEGACY_FLAGS[*]})"
for t in "${LEGACY_TESTS[@]}"; do run_cpp "$t" "${LEGACY_FLAGS[@]}"; done
echo "== 2026-10-31 native cores (g++ ${STRICT_FLAGS[*]})"
for t in "${STRICT_TESTS[@]}"; do run_cpp "$t" "${STRICT_FLAGS[@]}"; done

echo "== exception-free header checks (-fno-exceptions -fno-rtti)"
for h in "${STRICT_HEADERS[@]}"; do
  tu="$OUT/include-${h%.h}.cpp"
  printf '#include "%s"\nint main(){return 0;}\n' "$h" >"$tu"
  if "$CXX" "${STRICT_FLAGS[@]}" -fno-exceptions -fno-rtti -I"$SRC" "$tu" -o "$OUT/include-${h%.h}" >"$OUT/include-${h%.h}.log" 2>&1
  then record "header:$h" PASS "compiles without exceptions/RTTI"
  else record "header:$h" FAIL "see include-${h%.h}.log"; fi
done

echo "== Python tools"
for t in "${PY_TESTS[@]}"; do
  if python3 "$ROOT/Tests/$t" >"$OUT/$t.log" 2>&1; then record "$t" PASS "$(grep -E '^(Ran|OK|[0-9]+ checks)' "$OUT/$t.log" | tr '\n' ' ')"
  else record "$t" FAIL "see $t.log"; fi
done

{
  printf '{\n  "ok": %s,\n  "scope": "g++ native cores and Python tools only; no Unreal build, MSVC, physical controller, other PC, network or packaged runtime",\n' "$([ $fail -eq 0 ] && echo true || echo false)"
  printf '  "compiler": "%s",\n  "passed": %d,\n  "failed": %d,\n  "results": [\n' "$("$CXX" --version | head -n 1 | sed 's/"/\\"/g')" "$pass" "$fail"
  for i in "${!rows[@]}"; do
    IFS='|' read -r n s d <<<"${rows[$i]}"
    d="${d//\\/\\\\}"; d="${d//\"/\\\"}"
    printf '    {"name": "%s", "status": "%s", "detail": "%s"}%s\n' "$n" "$s" "$d" "$([ "$i" -lt $((${#rows[@]}-1)) ] && echo ,)"
  done
  printf '  ]\n}\n'
} >"$OUT/result.json"
echo "== $pass passed, $fail failed. Result: $OUT/result.json"
[ "$fail" -eq 0 ]
