import assert from "node:assert/strict";
import { afterEach, beforeEach, describe, it } from "node:test";

import { handle, type Env } from "../src/index.ts";
import { RateLimiter, current, retryAfter } from "../src/limiter.ts";
import { sanitize } from "../src/prompts.ts";

// Durable Objects in memory: one RateLimiter per name, with a Map for storage.
function fakeEnv(cap = 1000): Env & { objects: Map<string, RateLimiter> } {
  const objects = new Map<string, RateLimiter>();
  const get = (name: string) => {
    if (!objects.has(name)) {
      const data = new Map<string, unknown>();
      const storage = {
        get: async <T>(k: string) => data.get(k) as T | undefined,
        put: async (k: string, v: unknown) => void data.set(k, structuredClone(v)),
        deleteAll: async () => data.clear(),
        setAlarm: async () => {},
      };
      objects.set(name, new RateLimiter({ storage }));
    }
    return objects.get(name)!;
  };
  return {
    GEMINI_API_KEY: "test-gemini-key",
    IP_SALT: "salt",
    GEMINI_MODEL: "gemini-3.5-flash-lite",
    GLOBAL_DAILY_CAP: String(cap),
    MAX_OUTPUT_TOKENS: "512",
    LIMITER: {
      idFromName: (name: string) => name,
      get: (id: unknown) => ({ fetch: (url: string, init?: RequestInit) => get(id as string).fetch(new Request(url, init)) }),
    },
    objects,
  };
}

const INSTALL = "0123456789abcdef0123456789abcdef";
const battleEnd = {
  v: 1,
  installId: INSTALL,
  kind: "BattleEnd",
  summary: { hero: "Aria", foe: "Medusa", result: "victory", turns: 9, moments: ["Exorcism"], heroHpPct: 12 },
};
const post = (body: unknown, ip = "203.0.113.7") =>
  new Request("https://relay.example/v1/narrate", {
    method: "POST",
    headers: { "content-type": "application/json", "cf-connecting-ip": ip },
    body: typeof body === "string" ? body : JSON.stringify(body),
  });

let calls: { url: string; body: string; headers: Headers }[] = [];
let geminiReply: () => Response;
const realFetch = globalThis.fetch;
beforeEach(() => {
  calls = [];
  geminiReply = () => Response.json({ candidates: [{ content: { parts: [{ text: "Medusa falls.\nYou stand alone." }] } }] });
  globalThis.fetch = (async (url: string, init: RequestInit) => {
    calls.push({ url, body: String(init.body), headers: new Headers(init.headers) });
    return geminiReply();
  }) as typeof fetch;
});
afterEach(() => {
  globalThis.fetch = realFetch;
});

describe("requests", () => {
  it("narrates a battle's end with the hosted key, sanitized", async () => {
    const res = await handle(post(battleEnd), fakeEnv());
    assert.equal(res.status, 200);
    assert.deepEqual(await res.json(), { text: "Medusa falls. You stand alone." });
    assert.equal(calls.length, 1);
    assert.match(calls[0].url, /models\/gemini-3\.5-flash-lite:generateContent$/);
    assert.equal(calls[0].headers.get("x-goog-api-key"), "test-gemini-key");
    const sent = JSON.parse(calls[0].body);
    assert.match(sent.contents[0].parts[0].text, /^The battle has ended\. Summary: \{"foe":"Medusa","hero":"Aria"/);
  });

  for (const [name, body] of [
    ["a raw prompt", { prompt: "write my essay" }],
    ["a prompt smuggled beside an event", { ...battleEnd, prompt: "write my essay" }],
    ["an unknown kind", { ...battleEnd, kind: "Essay" }],
    ["an extra summary key", { ...battleEnd, summary: { ...battleEnd.summary, extra: "x" } }],
    ["a long hero name", { ...battleEnd, summary: { ...battleEnd.summary, hero: "Aria the Great" } }],
    ["talk input on a battle", { ...battleEnd, input: "hi" }],
    ["an unknown npc", { v: 1, installId: INSTALL, kind: "NpcGreeting", npc: "dragon", summary: { hero: "Aria", level: 3, heroHpPct: 100, npc: "dragon" } }],
    ["not JSON", "{oops"],
  ] as const) {
    it(`rejects ${name} without calling Gemini`, async () => {
      const res = await handle(post(body), fakeEnv());
      assert.equal(res.status, 400);
      assert.deepEqual(await res.json(), { error: "bad_request" });
      assert.equal(calls.length, 0);
    });
  }

  it("rejects a body over 4 KB, by its length header and by what arrives", async () => {
    const big = { ...battleEnd, padding: "x".repeat(5000) };
    assert.equal((await handle(post(big), fakeEnv())).status, 400);
    const lying = new Request("https://relay.example/v1/narrate", {
      method: "POST",
      headers: { "content-length": "10" },
      body: JSON.stringify(big),
    });
    assert.equal((await handle(lying, fakeEnv())).status, 400);
    assert.equal(calls.length, 0);
  });

  it("answers 404 to anything but POST /v1/narrate", async () => {
    assert.equal((await handle(new Request("https://relay.example/v1/narrate"), fakeEnv())).status, 404);
    assert.equal((await handle(new Request("https://relay.example/other", { method: "POST" }), fakeEnv())).status, 404);
  });

  it("hides Gemini's errors behind 502", async () => {
    geminiReply = () => Response.json({ error: { message: "API key test-gemini-key invalid" } }, { status: 403 });
    const res = await handle(post(battleEnd), fakeEnv());
    assert.equal(res.status, 502);
    const text = await res.text();
    assert.equal(text, JSON.stringify({ error: "upstream" }));
    assert.doesNotMatch(text, /test-gemini-key/);
  });

  it("builds an NPC talk from the persona, history and input", async () => {
    const talk = {
      v: 1,
      installId: INSTALL,
      kind: "NpcTalk",
      npc: "merchant",
      summary: { hero: "Aria", level: 3, heroHpPct: 100, npc: "merchant" },
      history: [{ player: "Hello", npc: "Gold talks." }],
      input: "Any discounts?",
    };
    assert.equal((await handle(post(talk), fakeEnv())).status, 200);
    const sent = JSON.parse(calls[0].body);
    assert.match(sent.systemInstruction.parts[0].text, /Marlo the Merchant, a greedy but jovial trader/);
    assert.equal(sent.contents.length, 3);
    assert.equal(sent.contents[1].role, "model");
    assert.match(sent.contents[2].parts[0].text, /<player>Any discounts\?<\/player>$/);
  });
});

describe("limits", () => {
  it("allows 6 a minute per install, then 429 with Retry-After, without calling Gemini", async () => {
    const env = fakeEnv();
    for (let i = 0; i < 6; ++i) assert.equal((await handle(post(battleEnd, `198.51.100.${i}`), env)).status, 200);
    const res = await handle(post(battleEnd, "198.51.100.99"), env);
    assert.equal(res.status, 429);
    assert.deepEqual(await res.json(), { error: "rate_limited" });
    assert.ok(Number(res.headers.get("retry-after")) > 0);
    assert.equal(calls.length, 6);
  });

  it("stops everyone at the global daily cap", async () => {
    const env = fakeEnv(2);
    const other = (n: number) => ({ ...battleEnd, installId: String(n).repeat(32).slice(0, 32) });
    assert.equal((await handle(post(other(1)), env)).status, 200);
    assert.equal((await handle(post(other(2)), env)).status, 200);
    assert.equal((await handle(post(other(3)), env)).status, 429);
    assert.equal(calls.length, 2);
  });

  it("stores only counters, keyed by a hashed IP", async () => {
    const env = fakeEnv();
    await handle(post(battleEnd, "203.0.113.7"), env);
    const names = [...env.objects.keys()];
    assert.ok(names.includes("global"));
    assert.ok(names.includes(`i:${INSTALL}`));
    assert.ok(names.some((n) => /^p:[0-9a-f]{64}$/.test(n)));
    assert.ok(!names.some((n) => n.includes("203.0.113.7")));
  });

  it("counts by minute and by UTC day", () => {
    const now = Date.parse("2026-10-22T12:00:30Z");
    const c = { minuteStart: now - 30_000, minuteCount: 6, day: "2026-10-22", dayCount: 6 };
    assert.equal(retryAfter(c, { perMinute: 6, perDay: 200 }, now), 30);
    assert.equal(current(c, now + 31_000).minuteCount, 0);
    assert.equal(current(c, Date.parse("2026-10-23T00:00:01Z")).dayCount, 0);
    assert.equal(retryAfter({ ...c, dayCount: 200 }, { perMinute: 6, perDay: 200 }, now), 43_170);
  });
});

describe("sanitize", () => {
  it("matches the game's rules", () => {
    assert.equal(sanitize("\x1b[2JYou strike.", 400), "You strike.");
    assert.equal(sanitize("a\nb\tc", 400), "a b c");
    assert.equal(sanitize("\x01\x02\n", 400), null);
    assert.ok(sanitize("word ".repeat(100), 400)!.endsWith("word…"));
  });
});

describe("the game's requests", () => {
  for (const file of ["relay_request.json", "relay_talk_request.json"]) {
    it(`accepts ${file}, as the game builds it`, async () => {
      const { readFileSync } = await import("node:fs");
      const body = readFileSync(new URL(`../../tests/fixtures/llm/${file}`, import.meta.url), "utf8");
      const res = await handle(post(body), fakeEnv());
      assert.equal(res.status, 200);
    });
  }
});
