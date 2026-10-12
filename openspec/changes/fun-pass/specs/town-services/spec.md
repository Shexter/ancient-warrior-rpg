# Spec Delta

## MODIFIED Requirements

### Requirement: Guildmaster
On entry, the guildmaster SHALL revive a hero at 0 HP to `min(100, max HP)` HP for `min(100, gold)` gold, so a broke hero is never stuck. The guildmaster SHALL offer:
- Restoration: use a consumable from the bag.
- Blessings: spend one blessing point for +30 max HP, +10 max MP, +6 ATK, or +3 DEF (raising current HP or MP by the same amount), refused without a point or past the stat cap.
- Charisma (level 10+): spend EXP in units of 50 for `points × 100` gold to raise max HP, max MP, ATK, or DEF by `points × 10 × path multiplier / level`.
- Learn Skill (level 5+ and the skill's level): costs its gold and one key item.
- Change Name: 50,000 gold.

#### Scenario: Revive with no gold
- **WHEN** a hero at 0 HP with 40 gold enters the guildmaster
- **THEN** the hero is revived to 100 HP and has 0 gold

#### Scenario: Learn skill consumes key
- **WHEN** a level 15 hero with 20,000 gold and a Broken Staff learns Frozen Throne
- **THEN** the skill is learned, 20,000 gold is spent, and the Broken Staff is consumed

#### Scenario: Worthless sacrifice refused
- **WHEN** a level 10 hero offers 30 EXP for Charisma
- **THEN** the request is refused and no EXP or gold is spent

#### Scenario: Spending a blessing
- **WHEN** a hero with 2 blessing points and 200 of 350 HP blesses Max HP
- **THEN** max HP is 380, HP is 230, and 1 point remains

#### Scenario: No blessing points
- **WHEN** a hero with 0 blessing points blesses ATK
- **THEN** the request is refused and nothing changes
