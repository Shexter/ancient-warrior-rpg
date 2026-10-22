# add-glyph-renderer notes

## 2026-10-21: hero models approved (task 3.2)

The user approved the turntable contact sheet: "Fighter = Knight, Rogue = Rogue, Hunter = Rogue_Hooded with the crossbow. Recolour to the path palettes, and keep battle sprites for monsters for now."

- Recoloured: Hunter's teal cloth to green (hue 137°), Pilgrim's to purple (hue 275°). Knight's crimson cape already matches the Fighter sprite, so it's unchanged.
- The files are about 3.7 MB each, mostly KayKit's 76 animation clips. Only four clips are used; trimming the rest would shrink them if size matters.
- Looks dim at game scale, with grey armour reading as grey. Lighting is tuned in 3.3, where the real renderer is built.
