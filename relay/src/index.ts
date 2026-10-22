// The narration relay: POST /v1/narrate with a structured game event, answered by Gemini with the
// hosted key (reference/llm-interfaces.md §8). It logs only {kind, status, ms}.
import { RateLimiter, type Limits } from "./limiter.ts";
import { buildPrompt, narrationChars, npcIds, sanitize } from "./prompts.ts";
import { parseRequest, type NarrateRequest } from "./schema.ts";

export { RateLimiter };

interface LimiterStub {
  fetch(url: string, init?: RequestInit): Promise<Response>;
}
export interface Env {
  GEMINI_API_KEY: string;
  IP_SALT: string;
  GEMINI_MODEL: string;
  GLOBAL_DAILY_CAP: string;
  MAX_OUTPUT_TOKENS: string;
  LIMITER: { idFromName(name: string): unknown; get(id: unknown): LimiterStub };
}

const MAX_BODY = 4096;
const json = (status: number, body: unknown, headers: Record<string, string> = {}) =>
  Response.json(body, { status, headers });
const badRequest = () => json(400, { error: "bad_request" });

async function sha256Hex(text: string): Promise<string> {
  const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(text));
  return [...new Uint8Array(digest)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

// Global, then per install, then per IP; counts the request only when all three allow it.
// ponytail: check-then-commit across three objects can let a burst slip a request or two past a
// limit; fine for a quota guard.
async function limit(env: Env, r: NarrateRequest, ip: string): Promise<number> {
  const keys: [string, Limits][] = [
    ["global", { perMinute: 0, perDay: Number(env.GLOBAL_DAILY_CAP) }],
    [`i:${r.installId}`, { perMinute: 6, perDay: 200 }],
    [`p:${await sha256Hex(env.IP_SALT + ip)}`, { perMinute: 20, perDay: 600 }],
  ];
  const stubs = keys.map(([name]) => env.LIMITER.get(env.LIMITER.idFromName(name)));
  for (let i = 0; i < keys.length; ++i) {
    const res = await stubs[i].fetch("https://limiter/check", { method: "POST", body: JSON.stringify(keys[i][1]) });
    const { retryAfter } = (await res.json()) as { retryAfter: number };
    if (retryAfter > 0) return retryAfter;
  }
  await Promise.all(stubs.map((s) => s.fetch("https://limiter/commit", { method: "POST" })));
  return 0;
}

async function askGemini(env: Env, r: NarrateRequest): Promise<string | null> {
  const { system, turns } = buildPrompt(r);
  const body = {
    systemInstruction: { parts: [{ text: system }] },
    contents: turns.map((t) => ({ role: t.fromPlayer ? "user" : "model", parts: [{ text: t.text }] })),
    generationConfig: { maxOutputTokens: Number(env.MAX_OUTPUT_TOKENS) },
  };
  try {
    const res = await fetch(
      `https://generativelanguage.googleapis.com/v1beta/models/${env.GEMINI_MODEL}:generateContent`,
      {
        method: "POST",
        headers: { "content-type": "application/json", "x-goog-api-key": env.GEMINI_API_KEY },
        body: JSON.stringify(body),
        signal: AbortSignal.timeout(7000),
      },
    );
    if (res.status !== 200) return null;
    const reply = (await res.json()) as {
      promptFeedback?: { blockReason?: string };
      candidates?: { finishReason?: string; content?: { parts?: { text?: string }[] } }[];
    };
    const first = reply.candidates?.[0];
    if (reply.promptFeedback?.blockReason || !first || first.finishReason === "SAFETY") return null;
    return sanitize((first.content?.parts ?? []).map((p) => p.text ?? "").join(""), narrationChars);
  } catch {
    return null; // timeout, network, or a malformed reply
  }
}

export async function handle(request: Request, env: Env): Promise<Response> {
  if (request.method !== "POST" || new URL(request.url).pathname !== "/v1/narrate")
    return new Response(null, { status: 404 });
  if (Number(request.headers.get("content-length") ?? "0") > MAX_BODY) return badRequest();
  const raw = await request.arrayBuffer();
  if (raw.byteLength > MAX_BODY) return badRequest();
  let body: unknown;
  try {
    body = JSON.parse(new TextDecoder().decode(raw));
  } catch {
    return badRequest();
  }
  const parsed = parseRequest(body, npcIds);
  if (!parsed) return badRequest();
  const wait = await limit(env, parsed, request.headers.get("cf-connecting-ip") ?? "");
  if (wait > 0) return json(429, { error: "rate_limited" }, { "retry-after": String(wait) });
  const text = await askGemini(env, parsed);
  return text === null ? json(502, { error: "upstream" }) : json(200, { text });
}

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    const start = Date.now();
    const copy = request.clone(); // for the kind, read after the handler has the body
    const response = await handle(request, env);
    let kind: unknown = "";
    try {
      kind = (JSON.parse(await copy.text()) as { kind?: unknown }).kind;
    } catch {
      // not JSON: nothing to log but the status
    }
    console.log({ kind: typeof kind === "string" ? kind.slice(0, 20) : "", status: response.status, ms: Date.now() - start });
    return response;
  },
};
