#include "enemy.h"
#include "character.h"
#include <algorithm>
#include <cstdlib>
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
  hp = amount;
  max_hp = amount;
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
void enemy::setdogeRate(int rate) { dogeRate = rate; }
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
      affixes.push_back(getRandomAffix());
      // 有词缀：经验和金币额外奖励
      resetexp_reward(getexp_reward() + getlevel() * 3);
      resetcoin_reward(getcoin_reward() + getlevel() * 2);
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
  hp += amount;
  if (hp > max_hp)
    hp = max_hp;
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
}

bool enemy::isBoss() const { return bossEncounter; }

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

EnemyIntent enemy::getIntent() const {
  EnemyIntent intent;
  if (encounterFloor < 40)
    return intent;

  unsigned int phase = completedTurns % 3;
  if (phase == 0)
    return intent;
  if (phase == 2) {
    intent.kind = EnemyIntentKind::Recover;
    intent.damagePercent = 0;
    return intent;
  }

  // Ordinary encounters retain one specialty. Late bosses rotate specialties
  // between cycles, while preserving the same visible three-turn rhythm.
  unsigned int strategy = (encounterFloor - 40) % 3;
  if (bossEncounter && encounterFloor >= 60)
    strategy = (completedTurns / 3) % 3;

  switch (strategy) {
  case 0:
    intent.kind = EnemyIntentKind::Wound;
    intent.healingPercent = 50;
    break;
  case 1:
    intent.kind = EnemyIntentKind::Shatter;
    intent.damagePercent = 80;
    intent.piercesGuard = true;
    break;
  case 2:
    intent.kind = EnemyIntentKind::ReadPattern;
    // The caller raises damage to 150% only if this prediction is matched.
    intent.damagePercent = 75;
    intent.predictedAction = predictAction();
    break;
  }
  return intent;
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
  case EnemyIntentKind::HeavyStrike:
    cout << "蓄力重击：造成 " << intent.damagePercent
         << "% 攻击伤害，可格挡。";
    break;
  case EnemyIntentKind::Wound:
    cout << "压制治疗：本次玩家行动的治疗仅生效 50%，随后普通攻击；"
            "下回合治疗恢复正常。";
    break;
  case EnemyIntentKind::Shatter:
    cout << "破防攻击：造成 80% 攻击伤害，无视本回合格挡。";
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

void enemy::finishTurn(int actionId) {
  if (actionId < -2 || actionId > 5)
    return;
  if (lastAction >= -2 && lastAction <= 5)
    ++actionTransitions[lastAction + 2][actionId + 2];
  lastAction = actionId;
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
               : max(1, baseAttack * (100 - attackWeakenPercent) / 100);
}

void enemy::weakenAttack(int percent, int turns) {
  if (percent <= 0 || turns <= 0)
    return;
  attackWeakenPercent = max(attackWeakenPercent, min(percent, 60));
  attackWeakenTurns = max(attackWeakenTurns, turns);
  updateWeakenedAttack();
}
