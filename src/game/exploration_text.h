#pragma once

// Chapter-themed names and texts for exploration spots, and the expedition
// journal. web/js/rules/exploration_text.js holds the same tables; the parity
// fingerprint covers them.
namespace ExplorationText {
// Ingredients: 0 fish, 1 herb, 2 ore, 3 frost crystal, 4 stardust.
struct ChapterText {
  const char *cache;
  const char *hiddenCache;
  const char *curios[3]; // Container, shrine, relic.
  const char *curioDescriptions[3];
  const char *gather[2];
  const char *gatherDescriptions[2];
  int gatherIngredient[2];
  const char *affixSource;
  const char *affixSourceDescription;
  const char *questItems[3];
  const char *vaults[2]; // One key, two keys.
  const char *puzzleTitle;
  const char *puzzlePrompt;
  const char *puzzleElements[5];
};
const ChapterText &chapter(int chapter);

// Fork floors: the fog on the far branch and its source, the high ground, the
// two kinds of shortcut and the stash behind the key gate.
struct ForkText {
  const char *fog;
  const char *fogSource;
  const char *fogSourceDescription;
  const char *vantage;
  const char *vantageDescription;
  const char *shortcuts[2]; // Crack, bridge lever.
  const char *stash;
};
const ForkText &fork(int chapter);

struct LorePage {
  const char *title;
  const char *text;
};
// Pages 1..100, one per floor.
const LorePage &lore(int page);
} // namespace ExplorationText
