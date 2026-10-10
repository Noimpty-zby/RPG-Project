#include "enemy.h"
#include "character.h"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <ostream>
using namespace std;
#include <iostream>
enemy::enemy(string name, int hp, int attack, int mp, int exp_reward,
             int coin_reward, int level)
    : Character(name, hp, attack, mp), exp_reward(exp_reward),
      coin_reward(coin_reward), level(level), baseAttack(attack) {}
int enemy::getexp_reward() const { return exp_reward; }
void enemy::takedamage(int damage) {
  int dodgeChance = rand() % 100;
  if (dodgeChance < dogeRate) {
    cout << getname() << "闪避了攻击！" << endl;
    return; // 闪避成功，不受伤害
  }
  Character::takedamage(damage);
}
int enemy::getcoin_reward() const { return coin_reward; }
int enemy::getlevel() const { return level; }
void enemy::resethp(int amount) {
  hp = max(0, amount);
  max_hp = hp;
}
void enemy::resetexp_reward(int amount) { exp_reward = amount; }
void enemy::resetcoin_reward(int amount) { coin_reward = amount; }
void enemy::resetattack(int amount) {
  baseAttack = max(0, amount);
  attackWeakenPercent = 0;
  attackWeakenTurns = 0;
  attack = baseAttack;
}
void enemy::resetall(int hp, int attack, int mp, int exp_reward,
                     int coin_reward) {
  resethp(hp);
  resetattack(attack);
  this->mp = mp;
  resetexp_reward(exp_reward);
  resetcoin_reward(coin_reward);
}
int enemy::getdogeRate() const { return dogeRate; }
void enemy::setdogeRate(int rate) { dogeRate = clamp(rate, 0, 100); }
Affix enemy::getRandomAffix() const {
  // 重新设计概率权重：危险词缀稀有但恐怖，普通词缀常见
  int r = rand() % 100;
  if (r < 25)
    return Affix::STRONG; // 25% 力量：攻击提升
  else if (r < 45)
    return Affix::TOUGH; // 20% 坚韧：HP提升
  else if (r < 60)
    return Affix::QUICK; // 15% 迅捷：闪避提升
  else if (r < 73)
    return Affix::POSIONOUS; // 13% 剧毒：持续伤害（危险）
  else if (r < 85)
    return Affix::HEALING; // 12% 治疗：战斗中回血（烦人）
  else if (r < 93)
    return Affix::EXPLOSIVE; //  8% 爆炸：死亡时反伤（惊喜）
  else
    return Affix::SHIELDING; //  7% 护盾：吸收伤害（最难缠）
}
void enemy::setAffix(int floor) {
  int generate_chance = 0;
  if (floor >= 1 && floor < 10) {
    generate_chance = 1;
  }
  if (floor >= 10 && floor < 20) {
    generate_chance = 2;
  }
  if (floor >= 20 && floor < 40) {
    generate_chance = 3;
  }
  if (floor >= 40 && floor < 60) {
    generate_chance = 4;
  }
  if (floor >= 60 && floor < 80) {
    generate_chance = 5;
  }
  if (floor >= 80 && floor <= 100) {
    generate_chance = 6;
  }
  // 每次有 40% 基础概率触发词缀，高楼层概率更高
  for (int i = 0; i < generate_chance; i++) {
    int threshold = min(40 + floor / 5, 70); // 40%~70%，随楼层增加
    if ((rand() % 100) < threshold) {
      const Affix rolled = getRandomAffix();
      if (hasAffix(rolled)) continue;
      affixes.push_back(rolled);
      // Only a newly added affix grants a bonus; duplicate rolls do nothing.
      const long long bonusLevel = max(0, getlevel());
      resetexp_reward(static_cast<int>(clamp(static_cast<long long>(getexp_reward()) + bonusLevel * 3,
                                             0LL, static_cast<long long>(numeric_limits<int>::max()))));
      resetcoin_reward(static_cast<int>(clamp(static_cast<long long>(getcoin_reward()) + bonusLevel * 2,
                                              0LL, static_cast<long long>(numeric_limits<int>::max()))));
    }
  }
}
bool enemy::hasAffix(Affix affix) const {
  for (const auto &a : affixes) {
    if (a == affix) {
      return true;
    }
  }
  return false;
}
void enemy::removeAffix(Affix affix) {
  affixes.erase(remove(affixes.begin(), affixes.end(), affix), affixes.end());
}
void enemy::displayAffixes() const {
  if (affixes.empty()) {
    cout << "[普通]";
    return;
  }
  cout << "敌人被加持了！！！【词缀】" << endl;
  for (auto aff : affixes) {
    switch (aff) {
    case Affix::STRONG:
      cout << "[强力] ";
      break;
    case Affix::TOUGH:
      cout << "[坚韧] ";
      break;
    case Affix::QUICK:
      cout << "[迅捷] ";
      break;
    case Affix::EXPLOSIVE:
      cout << "[爆炸] ";
      break;
    case Affix::POSIONOUS:
      cout << "[剧毒] ";
      break;
    case Affix::HEALING:
      cout << "[治疗] ";
      break;
    case Affix::SHIELDING:
      cout << "[护盾] ";
      break;
    }
  }
  cout << endl;
}
void enemy::heal(int amount) {
  if (amount <= 0 || !isalive()) return;
  hp = static_cast<int>(min(static_cast<long long>(hp) + amount,
                            static_cast<long long>(max_hp)));
}

void enemy::configureEncounter(int floor, bool boss) {
  encounterFloor = max(0, floor);
  bossEncounter = boss;
  completedTurns = 0;
  lastAction = -99;
  actionTransitions = {};
  attackWeakenPercent = 0;
  attackWeakenTurns = 0;
  attack = baseAttack;
  tacticalPressure = 0;
  specialCooldown = 0;
  turnsSinceSpecial = 0;
  repeatedActions = 0;
  recoveryTurns = 0;
  chargePending = false;
  interruptedThisTurn = false;
  lastPlayerHealed = false;
  desperationUsed = false;
  objectivePressure = false;
  sabotaged = false;
}

bool enemy::isBoss() const { return bossEncounter; }

namespace {
struct BossProfile {
  const char *title;
  EnemyIntentKind primary;
  EnemyIntentKind secondary;
  const char *hint;
};
const BossProfile bossProfiles[] = {
    {"荒野前哨·警钟巨兵", EnemyIntentKind::Charge, EnemyIntentKind::Charge,
     "目标：普攻打断巨兵的敲钟蓄力；错过窗口时格挡重击。"},
    {"幽林回廊·树心祭司", EnemyIntentKind::Wound, EnemyIntentKind::Charge,
     "目标：避开树心抑疗集中恢复，普攻打断仪式蓄力。"},
    {"沉没书库·禁书守卫", EnemyIntentKind::Shatter, EnemyIntentKind::Charge,
     "目标：守卫破防时主动出击，普攻打断蓄力，为学者争取时间。"},
    {"赤砂矿井·引爆监工", EnemyIntentKind::ReadPattern, EnemyIntentKind::Charge,
     "目标：改变监工预测的行动，用普攻打断引爆蓄力。"},
    {"寒霜关隘·双首冰兽", EnemyIntentKind::Wound, EnemyIntentKind::Shatter,
     "目标：避开冰霜抑疗安排恢复；连续格挡会招来破防。"},
    {"熔炉要塞·供能核心", EnemyIntentKind::Charge, EnemyIntentKind::Shatter,
     "目标：摧毁供能炉削弱核心，普攻打断蓄力，破防时主动出击。"},
    {"镜影剧场·镜阵导演", EnemyIntentKind::ReadPattern, EnemyIntentKind::Wound,
     "目标：打乱镜阵记录的连招，避免连续依赖回血。"},
    {"星蚀观测站·蚀星观测者", EnemyIntentKind::Shatter, EnemyIntentKind::ReadPattern,
     "目标：星蚀破防时主动出招，观测者预判时切换行动种类。"},
    {"王庭废墟·王座祭司", EnemyIntentKind::Wound, EnemyIntentKind::ReadPattern,
     "目标：避开王座抑疗安排恢复，防止回血习惯被祭司读招。"},
    {"天顶之门·远征裁决者", EnemyIntentKind::ReadPattern, EnemyIntentKind::Shatter,
     "目标：应对裁决者的读招、破防与低血蓄力，改变行动争取突围窗口。"}};

int chapterIndex(int floor) { return clamp((max(1, floor) - 1) / 10, 0, 9); }
} // namespace

string enemy::getEncounterTitle() const {
  if (bossEncounter) return bossProfiles[chapterIndex(encounterFloor)].title;
  switch (primaryTactic()) {
  case EnemyIntentKind::Charge: return "蓄力守卫";
  case EnemyIntentKind::Wound: return "抑疗猎手";
  case EnemyIntentKind::Shatter: return "破阵斥候";
  case EnemyIntentKind::ReadPattern: return "读招追猎者";
  default: return "普通敌人";
  }
}

string enemy::getTacticalHint() const {
  if (bossEncounter) return bossProfiles[chapterIndex(encounterFloor)].hint;
  return "观察早期意图：普攻命中可打断蓄力；格挡能抗重击，换招能避开预判。";
}

EnemyIntentKind enemy::primaryTactic() const {
  if (bossEncounter) return bossProfiles[chapterIndex(encounterFloor)].primary;
  const EnemyIntentKind ordinary[] = {
      EnemyIntentKind::Charge, EnemyIntentKind::Wound,
      EnemyIntentKind::Shatter, EnemyIntentKind::ReadPattern};
  return ordinary[max(0, encounterFloor) % 4];
}

EnemyIntentKind enemy::secondaryTactic() const {
  return bossEncounter ? bossProfiles[chapterIndex(encounterFloor)].secondary
                       : EnemyIntentKind::Charge;
}

int enemy::predictAction() const {
  if (lastAction < -2 || lastAction > 5)
    return -99;
  const auto &transitions = actionTransitions[lastAction + 2];
  unsigned int bestCount = 0;
  int prediction = -99;
  // Ties use the lower action ID, so the same history always gives the same tell.
  for (int i = 0; i < (int)transitions.size(); ++i) {
    if (transitions[i] > bestCount) {
      bestCount = transitions[i];
      prediction = i - 2;
    }
  }
  return prediction;
}

EnemyIntent enemy::makeIntent(EnemyIntentKind kind) const {
  EnemyIntent intent;
  intent.kind = kind;
  switch (kind) {
  case EnemyIntentKind::Charge:
    intent.damagePercent = 0;
    intent.interruptible = true;
    break;
  case EnemyIntentKind::HeavyStrike:
    intent.damagePercent = bossEncounter ? (sabotaged ? 125 : 160)
                                         : (sabotaged ? 110 : 135);
    break;
  case EnemyIntentKind::Wound:
    intent.damagePercent = 80;
    intent.healingPercent = 50;
    break;
  case EnemyIntentKind::Shatter:
    intent.damagePercent = 80;
    intent.piercesGuard = true;
    break;
  case EnemyIntentKind::ReadPattern:
    // The caller raises damage to 150% only if this prediction is matched.
    intent.damagePercent = 75;
    intent.predictedAction = predictAction();
    break;
  case EnemyIntentKind::Recover:
    intent.damagePercent = 0;
    break;
  case EnemyIntentKind::Attack:
    break;
  }
  return intent;
}

EnemyIntent enemy::getIntent() const {
  // Legacy/training enemies remain usable without configuring an encounter.
  if (encounterFloor == 0) return makeIntent(EnemyIntentKind::Attack);
  if (recoveryTurns > 0) return makeIntent(EnemyIntentKind::Recover);
  if (chargePending) return makeIntent(EnemyIntentKind::HeavyStrike);
  if (objectivePressure && specialCooldown == 0)
    return makeIntent(EnemyIntentKind::Charge);

  const auto primary = primaryTactic();
  const auto secondary = secondaryTactic();
  auto supports = [&](EnemyIntentKind kind) {
    return primary == kind || secondary == kind ||
           (bossEncounter && chapterIndex(encounterFloor) == 9);
  };
  if (specialCooldown == 0) {
    if (lastPlayerHealed && supports(EnemyIntentKind::Wound))
      return makeIntent(EnemyIntentKind::Wound);
    if (lastAction == -1 && supports(EnemyIntentKind::Shatter))
      return makeIntent(EnemyIntentKind::Shatter);
    if (supports(EnemyIntentKind::ReadPattern) && predictAction() != -99 &&
        (repeatedActions >= 2 || completedTurns >= 4))
      return makeIntent(EnemyIntentKind::ReadPattern);
    if (!desperationUsed && hp * 100LL <= max_hp * 35LL)
      return makeIntent(EnemyIntentKind::Charge);
    // One early tell introduces a small problem. Later choices depend on
    // player habits, enemy health and time since a special, not a global cycle.
    if ((bossEncounter && completedTurns == 0) ||
        (!bossEncounter && completedTurns == 1) ||
        turnsSinceSpecial >= max(2, 4 - tacticalPressure)) {
      auto tactic = primary;
      if (tactic == EnemyIntentKind::ReadPattern && predictAction() == -99)
        tactic = EnemyIntentKind::Charge;
      return makeIntent(tactic);
    }
  }
  return makeIntent(EnemyIntentKind::Attack);
}

namespace {
string actionDescription(int actionId) {
  if (actionId == 0)
    return "普通攻击";
  if (actionId == -1)
    return "格挡";
  if (actionId == -2)
    return "使用药水";
  if (actionId >= 1 && actionId <= 5)
    return "技能槽 " + to_string(actionId);
  return "尚未识别的行动";
}
} // namespace

void enemy::displayIntent() const {
  EnemyIntent intent = getIntent();
  cout << "【敌人意图】";
  switch (intent.kind) {
  case EnemyIntentKind::Attack:
    cout << "普通攻击：造成 100% 攻击伤害，可格挡。";
    break;
  case EnemyIntentKind::Charge:
    cout << "蓄力：本回合不攻击；普通攻击实际命中可打断并制造休整窗口。"
            "未打断则下回合重击，可用格挡或减攻应对。";
    break;
  case EnemyIntentKind::HeavyStrike:
    cout << "蓄力重击：造成 " << intent.damagePercent
         << "% 攻击伤害，可格挡。";
    break;
  case EnemyIntentKind::Wound:
    cout << "压制治疗：本次玩家行动治疗仅生效 50%，随后造成 80% 攻击伤害；"
            "可先普攻或格挡，离开此意图后治疗恢复正常。";
    break;
  case EnemyIntentKind::Shatter:
    cout << "破防攻击：造成 80% 攻击伤害，无视格挡减伤；"
            "可主动攻击或减攻，格挡仍能恢复资源。";
    break;
  case EnemyIntentKind::ReadPattern:
    if (intent.predictedAction == -99) {
      cout << "观察连招：经验不足，尚无法预测；本回合仅造成 75% 攻击伤害。";
    } else {
      cout << "预判连招：预计你使用【"
           << actionDescription(intent.predictedAction)
           << "】；命中预判造成 150% 攻击伤害，改变行动则仅造成 75%。";
    }
    break;
  case EnemyIntentKind::Recover:
    cout << "攻势恢复：本回合不发动攻击，治疗正常，可安排回血或准备爆发。";
    break;
  }
  cout << endl;
}

bool enemy::interruptCharge() {
  if (getIntent().kind != EnemyIntentKind::Charge) return false;
  chargePending = false;
  recoveryTurns = 1;
  interruptedThisTurn = true;
  specialCooldown = max(1, 3 - tacticalPressure) + (sabotaged ? 1 : 0);
  return true;
}

void enemy::setObjectivePressure(bool enabled) { objectivePressure = enabled; }
void enemy::setTacticalPressure(int difficulty) {
  tacticalPressure = clamp(difficulty, 0, 2);
}
void enemy::applySabotage() {
  removeAffix(Affix::SHIELDING);
  sabotaged = true;
}

void enemy::finishTurn(int actionId, bool healed) {
  finishTurn(actionId, healed, getIntent().kind);
}

void enemy::finishTurn(int actionId, bool healed, EnemyIntentKind resolved) {
  if (actionId < -2 || actionId > 5)
    return;
  if (interruptedThisTurn) {
    interruptedThisTurn = false;
    turnsSinceSpecial = 0;
  } else if (resolved == EnemyIntentKind::Charge) {
    chargePending = true;
    turnsSinceSpecial = 0;
  } else if (resolved == EnemyIntentKind::HeavyStrike) {
    chargePending = false;
    recoveryTurns = 1;
    specialCooldown = max(1, 3 - tacticalPressure) + (sabotaged ? 1 : 0);
    turnsSinceSpecial = 0;
  } else if (resolved == EnemyIntentKind::Wound ||
             resolved == EnemyIntentKind::Shatter ||
             resolved == EnemyIntentKind::ReadPattern) {
    specialCooldown = max(1, 3 - tacticalPressure) + (sabotaged ? 1 : 0);
    turnsSinceSpecial = 0;
  } else {
    if (resolved == EnemyIntentKind::Recover && recoveryTurns > 0)
      --recoveryTurns;
    if (specialCooldown > 0) --specialCooldown;
    ++turnsSinceSpecial;
  }
  if (resolved != EnemyIntentKind::Attack && hp * 100LL <= max_hp * 35LL)
    desperationUsed = true;
  if (lastAction >= -2 && lastAction <= 5)
    ++actionTransitions[lastAction + 2][actionId + 2];
  repeatedActions = actionId == lastAction ? repeatedActions + 1 : 1;
  lastAction = actionId;
  lastPlayerHealed = healed;
  ++completedTurns;
  if (attackWeakenTurns > 0) {
    --attackWeakenTurns;
    if (attackWeakenTurns == 0)
      attackWeakenPercent = 0;
    updateWeakenedAttack();
  }
}

void enemy::updateWeakenedAttack() {
  // Preserve zero-attack test/event enemies; all other attacks remain positive.
  attack = baseAttack == 0
               ? 0
               : static_cast<int>(max(1LL, static_cast<long long>(baseAttack) *
                                              (100 - attackWeakenPercent) / 100));
}

void enemy::weakenAttack(int percent, int turns) {
  if (percent <= 0 || turns <= 0)
    return;
  attackWeakenPercent = max(attackWeakenPercent, min(percent, 60));
  attackWeakenTurns = max(attackWeakenTurns, turns);
  updateWeakenedAttack();
}
