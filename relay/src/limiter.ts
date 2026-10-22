// Per-key request counters (one Durable Object per key: "global", "i:<installId>",
// "p:<hashed IP>"). Each keeps {minuteStart, minuteCount, day, dayCount} and deletes itself 48 h
// after it was last used. Only counters are ever stored.

export interface Counters {
  minuteStart: number; // ms
  minuteCount: number;
  day: string; // UTC date, "2026-10-22"
  dayCount: number;
}
export interface Limits {
  perMinute: number; // 0 = no per-minute limit
  perDay: number;
}

const MINUTE = 60_000;
const utcDay = (now: number) => new Date(now).toISOString().slice(0, 10);

// The counters as they stand at `now` (a new minute or day resets its count).
export function current(c: Counters | undefined, now: number): Counters {
  const fresh: Counters = c ?? { minuteStart: now, minuteCount: 0, day: utcDay(now), dayCount: 0 };
  const minute = now - fresh.minuteStart >= MINUTE ? { minuteStart: now, minuteCount: 0 } : fresh;
  const day = fresh.day !== utcDay(now) ? { day: utcDay(now), dayCount: 0 } : fresh;
  return { minuteStart: minute.minuteStart, minuteCount: minute.minuteCount, day: day.day, dayCount: day.dayCount };
}

// Seconds until the request would be allowed, or 0 when it is.
export function retryAfter(c: Counters, limits: Limits, now: number): number {
  if (limits.perDay > 0 && c.dayCount >= limits.perDay) {
    const midnight = Date.parse(`${c.day}T00:00:00Z`) + 86_400_000;
    return Math.max(1, Math.ceil((midnight - now) / 1000));
  }
  if (limits.perMinute > 0 && c.minuteCount >= limits.perMinute)
    return Math.max(1, Math.ceil((c.minuteStart + MINUTE - now) / 1000));
  return 0;
}

interface Storage {
  get<T>(key: string): Promise<T | undefined>;
  put(key: string, value: unknown): Promise<void>;
  deleteAll(): Promise<void>;
  setAlarm(when: number): Promise<void>;
}

// POST /check {perMinute, perDay} -> {retryAfter}; POST /commit counts one request.
export class RateLimiter {
  private readonly state: { storage: Storage };
  constructor(state: { storage: Storage }, _env?: unknown) {
    this.state = state;
  }

  async fetch(request: Request): Promise<Response> {
    const now = Date.now();
    const counters = current(await this.state.storage.get<Counters>("c"), now);
    const path = new URL(request.url).pathname;
    if (path === "/check") {
      const limits = (await request.json()) as Limits;
      return Response.json({ retryAfter: retryAfter(counters, limits, now) });
    }
    if (path === "/commit") {
      counters.minuteCount += 1;
      counters.dayCount += 1;
      await this.state.storage.put("c", counters);
      await this.state.storage.setAlarm(now + 48 * 3_600_000);
      return new Response(null, { status: 204 });
    }
    return new Response(null, { status: 404 });
  }

  async alarm(): Promise<void> {
    await this.state.storage.deleteAll();
  }
}
