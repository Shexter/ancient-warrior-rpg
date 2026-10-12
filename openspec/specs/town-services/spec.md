# town-services Specification

## Purpose
Provides the town NPCs that turn battle rewards into progress: buying, selling, and auctioning items, changing, upgrading, and forging gear, reviving and restoring the hero, buying stats and skills, and storing items.

## Requirements

### Requirement: All-or-nothing transactions
Every town action SHALL check all of its conditions before changing anything. A refused action SHALL leave the game state exactly as it was and SHALL tell the player why.

#### Scenario: Not enough gold
- **WHEN** the hero with 500 gold tries to buy a 1,000-gold Health Potion
- **THEN** the purchase is refused with a not-enough-gold message and gold and bag are unchanged

### Requirement: Merchant
The merchant SHALL sell and buy items that have a price, at that price, in quantities of 1–99, and SHALL refuse a dead hero. The merchant SHALL auction one unit of an item that has an auction value: with a 1-in-10 chance no buyer comes and the item is kept; otherwise bidding starts at the auction value and rises by 10% of it per accepted bid. The final price minus a 10% fee is paid.

#### Scenario: Auction cannot mint gold
- **WHEN** the hero auctions one Topaz (auction value 500)
- **THEN** either nobody buys it and the Topaz stays in the bag, or it is sold and the payout is between 450 and 855 gold; the player cannot choose the starting price

### Requirement: Changing equipment
The blacksmith SHALL equip a weapon or armor from the bag into the matching slot. The previously equipped item SHALL return to the bag at upgrade level 1, and the change SHALL be refused if the bag cannot hold it.

#### Scenario: Upgraded item resets
- **WHEN** the hero swaps an Iron Sword at upgrade level 3 for a Fire Claymore
- **THEN** the Iron Sword is in the bag at upgrade level 1 and the Fire Claymore is equipped at level 1

### Requirement: Upgrading equipment
Upgrading equipped gear at level L (1–9) SHALL cost L units of its upgrade material (Fire Dust for weapons, Turtle Shell for armor), `1000 × L` EXP, and `10000 × L` gold, and SHALL raise its level by 1, up to a maximum of 10.

#### Scenario: Missing material
- **WHEN** the hero has the gold and EXP for a level 2 weapon upgrade but only 1 Fire Dust
- **THEN** the upgrade is refused and nothing is deducted

### Requirement: Forging
Forging a recipe SHALL require all ingredients in the bag and room for the result. It SHALL consume the ingredients and succeed with probability `forge level × 10` percent. Success adds the result and raises forge level by 1 (max 10). Failure loses the ingredients.

#### Scenario: Missing ingredient
- **WHEN** the hero lacks a Phoenix Feather for the Fire Claymore recipe
- **THEN** forging is refused before any ingredient is consumed

### Requirement: Guildmaster
On entry, the guildmaster SHALL revive a hero at 0 HP to `min(100, max HP)` HP for `min(100, gold)` gold, so a broke hero is never stuck. The guildmaster SHALL offer:
- Restoration: use a consumable from the bag.
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

### Requirement: Bank
Opening a bank account SHALL cost 10,000 gold once and SHALL be refused without enough gold. The bank SHALL hold 10 slots with the same stacking rules as the bag, and items SHALL move between bag and bank without loss.

#### Scenario: Deposit and withdraw
- **WHEN** the hero deposits 3 Topaz and later withdraws them
- **THEN** the bag holds 3 Topaz and the bank holds none

#### Scenario: Cannot open without gold
- **WHEN** a hero with 5,000 gold tries to open an account
- **THEN** the account is not opened and gold is unchanged
