// Hand-written validation of a narration request (reference/llm-interfaces.md §8). Anything that
// isn't exactly a structured game event is rejected, so the relay can't be used as a free
// general-purpose model.

export const KINDS = ["BossIntro", "BattleMoment", "BattleEnd", "FloorEnter", "NpcGreeting", "NpcTalk"] as const;
export type Kind = (typeof KINDS)[number];

export interface TalkTurn {
  player: string;
  npc: string;
}
export interface NarrateRequest {
  v: 1;
  installId: string;
  kind: Kind;
  summary: Record<string, unknown>;
  npc?: string;
  history?: TalkTurn[];
  input?: string;
}

type Field = "string" | "int" | "strings" | readonly string[];
// Every key of each kind's summary, and its type: strings, integers, a list of strings, or one of
// a set of words.
const SUMMARIES: Record<Kind, Record<string, Field>> = {
  BossIntro: { hero: "string", path: "string", level: "int", foe: "string", location: "string" },
  BattleMoment: {
    hero: "string",
    foe: "string",
    moment: ["skill", "low_hp", "status"],
    detail: "string",
    heroHpPct: "int",
    foeHpPct: "int",
  },
  BattleEnd: {
    hero: "string",
    foe: "string",
    result: ["victory", "defeat", "fled", "draw"],
    turns: "int",
    moments: "strings",
    heroHpPct: "int",
  },
  FloorEnter: { hero: "string", floor: "int", floors: "int" },
  NpcGreeting: { hero: "string", level: "int", heroHpPct: "int", npc: "string" },
  NpcTalk: { hero: "string", level: "int", heroHpPct: "int", npc: "string" },
};
const TOP = new Set(["v", "installId", "kind", "summary", "npc", "history", "input"]);

const isObject = (x: unknown): x is Record<string, unknown> => typeof x === "object" && x !== null && !Array.isArray(x);
const printable = (s: string) => /^[\x20-\x7e]*$/.test(s);
const shortString = (x: unknown, max: number) => typeof x === "string" && x.length <= max;

function validSummary(kind: Kind, summary: unknown): boolean {
  if (!isObject(summary)) return false;
  const fields = SUMMARIES[kind];
  const keys = Object.keys(summary);
  if (keys.length !== Object.keys(fields).length) return false;
  for (const key of keys) {
    const type = fields[key];
    const value = summary[key];
    if (type === undefined) return false;
    if (type === "string" && !shortString(value, key === "hero" ? 8 : 40)) return false;
    if (type === "int" && !(Number.isInteger(value) && (value as number) >= 0 && (value as number) <= 1_000_000)) return false;
    if (type === "strings" && !(Array.isArray(value) && value.length <= 5 && value.every((m) => shortString(m, 40))))
      return false;
    if (Array.isArray(type) && !type.includes(value as string)) return false;
  }
  return true;
}

// The request, or null when it isn't one. `npcs` are the known NPC ids.
export function parseRequest(body: unknown, npcs: ReadonlySet<string>): NarrateRequest | null {
  if (!isObject(body)) return null;
  if (Object.keys(body).some((k) => !TOP.has(k))) return null; // prompt, messages, system, ...
  if (body.v !== 1) return null;
  if (typeof body.installId !== "string" || !/^[0-9a-f]{32}$/.test(body.installId)) return null;
  if (!KINDS.includes(body.kind as Kind)) return null;
  const kind = body.kind as Kind;
  if (!validSummary(kind, body.summary)) return null;
  const npcKind = kind === "NpcGreeting" || kind === "NpcTalk";
  if (npcKind ? typeof body.npc !== "string" || !npcs.has(body.npc) : body.npc !== undefined) return null;
  if (body.history !== undefined) {
    if (kind !== "NpcTalk" || !Array.isArray(body.history) || body.history.length > 6) return null;
    for (const turn of body.history)
      if (!isObject(turn) || Object.keys(turn).length !== 2 || !shortString(turn.player, 300) || !shortString(turn.npc, 300))
        return null;
  }
  if (body.input !== undefined && (kind !== "NpcTalk" || !shortString(body.input, 200) || !printable(body.input as string)))
    return null;
  return body as unknown as NarrateRequest;
}
