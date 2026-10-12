# Sprite art pipeline

How the shipped `assets/sprites/*.sprite` were made. Nothing here is part of the build.

1. Generate locally with Draw Things (FLUX.2 [klein] 4B, 512x512):
   ```
   draw-things-cli generate --model flux_2_klein_4b_q8p.ckpt --offline --width 512 --height 512 \
     --seed SEED --prompt "PROMPT" -o in.png
   ```
2. Remove the key background, crop, fit bottom-centered, quantize to 32 colors (needs Pillow and numpy):
   ```
   python3 -I tools/art/to_sprite_png.py in.png green|magenta WIDTH HEIGHT out.png
   ```
3. Bake: `./build/debug/sprite_bake out.png assets/sprites/NAME.sprite` for half-blocks, or `./build/debug/sprite_bake out.png NAME.glyphs --glyphs` for a glyph sprite (the same W x 2H PNG gives the same cell footprint in both looks).

Use a magenta background for anything green (Hunter's cloak, Medusa). Heroes face right; enemies face left.

Every prompt ends with this style suffix: *Retro 16-bit video game character sprite, flat cel shading, few colors, bold dark outline, centered, the whole figure visible with margin, no shadow, no ground, no text.* followed by *Solid pure green background (#00FF00).* or *Solid pure magenta background (#FF00FF).*

| sprite | size | seed | key | prompt (before the style suffix) |
|---|---|---|---|---|
| `hero_fighter` | 24x24 | 42 | green | Full body side view of a stocky medieval fighter facing right, standing in a ready stance. Red tunic, brown leather belt and boots, short iron sword in his right hand, round steel shield. |
| `hero_hunter` | 24x24 | 42 | magenta | Full body side view of a lean medieval hunter facing right, green hooded cloak, brown leather tunic, holding a wooden longbow, quiver on back. |
| `hero_rogue` | 24x24 | 42 | green | Full body side view of a slim medieval rogue facing right, deep purple hood and cloak, dark leather armor, a dagger in each hand, crouched ready stance. |
| `medusa` | 32x28 | 7 | magenta | Side view of Medusa the gorgon facing left: upper body of a woman with green-teal scaly skin and a head of writhing green snakes for hair, and below the waist one long coiled serpent tail (no legs). Menacing pose, clawed hands raised. |
| `monster` | 16x16 | 42 | green | Side view of a wild brown beast facing left, like a snarling boar-wolf on four legs, brown fur, small tusks. |
| `avaritia` | 32x28 | 7 | green | Side view of Avaritia, the demon of greed, facing left: a gaunt hunched figure in a heavy crimson and gold robe, grey skin, long clawed fingers clutching a bulging sack of gold coins, a chain of coins around its neck, a crown of bent coins, glowing yellow eyes, menacing pose. |

One exception: the fighter's suffix put the background before the last three phrases ("... margin, solid pure green background (#00FF00), no shadow, no ground, no text."). Same model, seed and exact prompt reproduce the image on the same Draw Things version.
