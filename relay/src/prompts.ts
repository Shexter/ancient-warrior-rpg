// The model's prompts, built exactly as the game builds them (reference/llm-interfaces.md §5),
// from src/prompts.json (synced from data/narrator.json), and the reply sanitizing (§6).
import data from "./prompts.json" with { type: "json" };
import type { NarrateRequest } from "./schema.ts";

export const npcIds: ReadonlySet<string> = new Set(Object.keys(data.npcs));
export const narrationChars: number = data.limits.narrationChars;

// JSON with keys sorted at every level, as the game's JSON library writes it.
function sortedJson(value: unknown): string {
  if (Array.isArray(value)) return `[${value.map(sortedJson).join(",")}]`;
  if (value !== null && typeof value === "object") {
    const o = value as Record<string, unknown>;
    return `{${Object.keys(o).sort().map((k) => `${JSON.stringify(k)}:${sortedJson(o[k])}`).join(",")}}`;
  }
  return JSON.stringify(value);
}

export interface Turn {
  fromPlayer: boolean;
  text: string;
}

export function buildPrompt(r: NarrateRequest): { system: string; turns: Turn[] } {
  const npcs = data.npcs as Record<string, { name: string; persona: string }>;
  const system =
    r.npc !== undefined
      ? data.prompts.npcSystem.replaceAll("{npcName}", npcs[r.npc].name).replaceAll("{persona}", npcs[r.npc].persona)
      : data.prompts.narratorSystem;
  const turns: Turn[] = [];
  for (const t of r.history ?? []) {
    turns.push({ fromPlayer: true, text: `<player>${t.player}</player>` });
    turns.push({ fromPlayer: false, text: t.npc });
  }
  const user = (data.prompts.user as Record<string, string>)[r.kind]
    .replaceAll("{summary}", sortedJson(r.summary))
    .replaceAll("{input}", r.input ?? "");
  turns.push({ fromPlayer: true, text: user });
  return { system, turns };
}

// Control bytes and escape sequences removed, whitespace collapsed, cut at the last space within
// `limit` characters with "…"; null when nothing printable is left.
export function sanitize(reply: string, limit: number): string | null {
  const clean = reply
    .replace(/\x1b\[[\x20-\x3f]*[\x40-\x7e]|\x1b./g, "")
    .replace(/[\n\r\t]/g, " ")
    .replace(/[\x00-\x1f\x7f]/g, "")
    .replace(/ {2,}/g, " ")
    .trim();
  if (clean === "") return null;
  const chars = [...clean];
  if (chars.length <= limit) return clean;
  const head = chars.slice(0, limit).join("");
  const space = head.lastIndexOf(" ");
  return (space > 0 ? head.slice(0, space) : head) + "…";
}
