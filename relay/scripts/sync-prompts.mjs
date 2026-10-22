// Copies {prompts, npcs, limits} from data/narrator.json into src/prompts.json, so the relay's
// prompts can't drift from the game's. Runs before tests and deploys.
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const narrator = JSON.parse(readFileSync(join(here, "../../data/narrator.json"), "utf8"));
const { prompts, npcs, limits } = narrator;
writeFileSync(join(here, "../src/prompts.json"), JSON.stringify({ prompts, npcs, limits }, null, 2) + "\n");
