#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
output=build-local
if [[ "${SANITIZE:-0}" == 1 ]]; then output=build-sanitize; fi
if [[ "${SANITIZE:-0}" == address ]]; then output=build-address; fi
mkdir -p "$output"
compiler="${CXX:-g++}"
flags=(-std=c++17 -O2 -Wall -Wextra -Wpedantic -Isrc)
if [[ "${SANITIZE:-0}" == 1 ]]; then flags+=(-O1 -g -fsanitize=undefined -fno-sanitize-recover=all); fi
if [[ "${SANITIZE:-0}" == address ]]; then flags+=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
sources=(src/core/random.cpp src/character/character.cpp src/player/player.cpp src/player/jobs.cpp src/player/skills.cpp
         src/enemy/enemy.cpp src/item/item.cpp src/game/campaign.cpp src/game/combat.cpp src/game/world.cpp src/game/exploration.cpp
         src/game/exploration_text.cpp
         src/storage/save.cpp src/storage/profile.cpp src/ui/help.cpp)
objects=()
for source in "${sources[@]}"; do
  object="$output/${source//\//_}.o"
  "$compiler" "${flags[@]}" -c "$source" -o "$object"
  objects+=("$object")
done
"$compiler" "${flags[@]}" src/main.cpp "${objects[@]}" -o "$output/RPG_Game"
for test_name in random_tests enemy_tests skill_progression_tests save_tests combat_tests build_tests loot_tests campaign_tests profile_tests exploration_tests; do
  "$compiler" "${flags[@]}" "tests/unit/$test_name.cpp" "${objects[@]}" -o "$output/$test_name"
  "$output/$test_name"
done
if command -v python3 >/dev/null 2>&1; then
  python3 tests/cli_smoke.py "$output/RPG_Game"
fi
"$compiler" "${flags[@]}" tests/sim/balance_sim.cpp "${objects[@]}" -o "$output/balance_sim"
"$output/balance_sim" > "$output/balance-results.csv"
printf 'Representative balance simulation saved to %s/balance-results.csv\n' "$output"
"$compiler" "${flags[@]}" tests/sim/campaign_sim.cpp "${objects[@]}" -o "$output/campaign_sim"
"$output/campaign_sim" --check > "$output/campaign-results.csv"
printf 'Continuous campaign simulation saved to %s/campaign-results.csv\n' "$output"
