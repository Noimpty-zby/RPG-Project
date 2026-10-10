#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
output=build-local
mkdir -p "$output"
compiler="${CXX:-g++}"
flags=(-std=c++17 -O2 -Wall -Wextra -Wpedantic -I. -Icharacter -Iplayer -Ienemy -Iitem)
sources=(character/character.cpp player/player.cpp player/jobs.cpp player/skills.cpp enemy/enemy.cpp item/item.cpp save.cpp combat.cpp world.cpp)
objects=()
for source in "${sources[@]}"; do
  object="$output/${source//\//_}.o"
  "$compiler" "${flags[@]}" -c "$source" -o "$object"
  objects+=("$object")
done
"$compiler" "${flags[@]}" main.cpp "${objects[@]}" -o "$output/RPG_Game"
for test_name in enemy_tests skill_progression_tests save_tests combat_tests; do
  "$compiler" "${flags[@]}" "tests/$test_name.cpp" "${objects[@]}" -o "$output/$test_name"
  "$output/$test_name"
done
if command -v python3 >/dev/null 2>&1; then
  python3 tests/cli_smoke.py "$output/RPG_Game"
fi
"$compiler" "${flags[@]}" tests/balance_sim.cpp "${objects[@]}" -o "$output/balance_sim"
"$output/balance_sim" > "$output/balance-results.csv"
printf 'Representative balance simulation saved to %s/balance-results.csv\n' "$output"
