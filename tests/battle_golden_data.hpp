// Expected event sequences copied verbatim from
// openspec/changes/build-core-engine/reference/golden-vectors.md. Do not edit to make a test pass:
// if the engine disagrees, stop and report.
#pragma once

#include <string_view>

inline constexpr std::string_view kGoldenA = R"(
TurnStarted Hero 1
Attacked Hero
Damaged Enemy 80 2920 Attack
TurnStarted Enemy 1
Attacked Enemy
Damaged Hero 225 125 Attack
Healed Enemy 10 2930
TurnStarted Hero 2
Attacked Hero
Damaged Enemy 80 2850 Attack
TurnStarted Enemy 2
Attacked Enemy
Damaged Hero 225 0 Attack
Defeated Hero
BattleEnded Defeat 2
)";

inline constexpr std::string_view kGoldenB = R"(
TurnStarted Hero 1
SkillCast frozen_throne 40
Damaged Enemy 380 2620 Skill
StatusApplied Enemy stun 3
StatusApplied Enemy defense_down 3
TurnStarted Enemy 1
TurnSkipped Enemy stun
TurnStarted Hero 2
SkillCast golden_bell 25
Damaged Enemy 390 2230 Skill
StatusApplied Hero reflect 2
TurnStarted Enemy 2
TurnSkipped Enemy stun
TurnStarted Hero 3
Attacked Hero
Damaged Enemy 390 1840 Attack
TurnStarted Enemy 3
TurnSkipped Enemy stun
StatusExpired Enemy stun
StatusExpired Enemy defense_down
TurnStarted Hero 4
Attacked Hero
Damaged Enemy 380 1460 Attack
StatusExpired Hero reflect
TurnStarted Enemy 4
Attacked Enemy
Damaged Hero 50 1950 Attack
Healed Enemy 10 1470
TurnStarted Hero 5
Attacked Hero
Damaged Enemy 380 1090 Attack
Healed Hero 50 2000
TurnStarted Enemy 5
Attacked Enemy
Damaged Hero 50 1950 Attack
Healed Enemy 10 1100
TurnStarted Hero 6
Attacked Hero
Damaged Enemy 380 720 Attack
Healed Hero 50 2000
TurnStarted Enemy 6
Attacked Enemy
Damaged Hero 50 1950 Attack
TurnStarted Hero 7
Attacked Hero
Damaged Enemy 380 340 Attack
Healed Hero 50 2000
TurnStarted Enemy 7
Attacked Enemy
Damaged Hero 50 1950 Attack
ExtraTurn Enemy
TurnStarted Enemy 7
Attacked Enemy
Damaged Hero 50 1900 Attack
Healed Enemy 10 350
TurnStarted Hero 8
Attacked Hero
Damaged Enemy 380 0 Attack
Defeated Enemy
BattleEnded Victory 8
)";

inline constexpr std::string_view kGoldenC = R"(
TurnStarted Hero 1
SkillCast golden_bell 25
Damaged Enemy 380 2620 Skill
StatusApplied Hero reflect 2
TurnStarted Enemy 1
Attacked Enemy
Damaged Hero 25 1975 Attack
Damaged Enemy 25 2595 Reflect
Healed Enemy 10 2605
TurnStarted Hero 2
Attacked Hero
Damaged Enemy 380 2225 Attack
Healed Hero 25 2000
TurnStarted Enemy 2
Attacked Enemy
Damaged Hero 25 1975 Attack
Damaged Enemy 25 2200 Reflect
Healed Enemy 10 2210
TurnStarted Hero 3
Attacked Hero
Damaged Enemy 380 1830 Attack
Healed Hero 25 2000
StatusExpired Hero reflect
TurnStarted Enemy 3
Attacked Enemy
Damaged Hero 50 1950 Attack
)";

inline constexpr std::string_view kGoldenD = R"(
TurnStarted Hero 1
SkillCast supernova 50
HpHalved Enemy 1500
Damaged Enemy 1500 0 Skill
Defeated Enemy
BattleEnded Victory 1
)";
