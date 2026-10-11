#!/usr/bin/env bash
# Checks that the browser rules in web/js/rules match the C++ rules exactly:
# the C++ campaign_sim output must equal the JavaScript port's, byte for byte.
set -euo pipefail
cd "$(dirname "$0")/.."
output=build-web
mkdir -p "$output"
compiler="${CXX:-g++}"
sources=(src/core/random.cpp src/character/character.cpp src/player/player.cpp src/player/jobs.cpp src/player/skills.cpp
         src/enemy/enemy.cpp src/item/item.cpp src/game/campaign.cpp src/game/combat.cpp src/game/world.cpp src/game/exploration.cpp
         src/game/exploration_text.cpp
         src/storage/save.cpp src/storage/profile.cpp src/ui/help.cpp)
"$compiler" -std=c++17 -O2 -Isrc "${sources[@]}" tests/sim/campaign_sim.cpp -o "$output/campaign_sim"
"$output/campaign_sim" > "$output/campaign-reference.csv"
node web/tests/parity_test.mjs "$output/campaign-reference.csv"
node web/tests/rules_test.mjs
