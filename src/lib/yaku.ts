/**
 * Yaku definitions and scoring, mirroring src/KoiKoiProfile.hpp,
 * src/KoiKoiRules.cpp (ScoreYaku / CalculateSettlement) exactly.
 */
import { CARDS, cardById, type CardType } from "./cards";

/** Constants from src/KoiKoiProfile.hpp – the KK-ADD-10 profile. */
export const PROFILE = {
  id: "KK-ADD-10",
  version: "xeno-f422bac",
  sourceCommit: "f422bacf2519cdfd1b7582a647015356478ce66b",
  goko: 10,
  shiko: 8,
  ameShiko: 7,
  sanko: 5,
  fixedSet: 5,
  animalThreshold: 5,
  ribbonThreshold: 4,
  plainThreshold: 10,
  sevenThreshold: 7,
  openingPoints: 6,
  maxCallerFactor: 6,
} as const;

export type YakuId =
  | "goko"
  | "shiko"
  | "ame-shiko"
  | "sanko"
  | "hanami"
  | "tsukimi"
  | "ino-shika-cho"
  | "akatan"
  | "aotan"
  | "akatan-aotan"
  | "tane"
  | "tanzaku"
  | "kasu";

export type YakuGroup = "brights" | "fixed" | "ribbons" | "counting";

export interface YakuDef {
  id: YakuId;
  name: string;
  kanji: string;
  group: YakuGroup;
  /** Fixed value, or the value at the threshold for counting yaku. */
  points: number;
  counting: boolean;
  description: string;
  /** Example / required cards (stable ids). */
  cards: string[];
}

export const YAKU: YakuDef[] = [
  {
    id: "goko",
    name: "Goko",
    kanji: "五光",
    group: "brights",
    points: PROFILE.goko,
    counting: false,
    description: "All five brights – the biggest yaku in the game.",
    cards: ["01-B", "03-B", "08-B", "11-B", "12-B"],
  },
  {
    id: "shiko",
    name: "Shiko",
    kanji: "四光",
    group: "brights",
    points: PROFILE.shiko,
    counting: false,
    description: "Four brights without the Rainman.",
    cards: ["01-B", "03-B", "08-B", "12-B"],
  },
  {
    id: "ame-shiko",
    name: "Ame-Shiko",
    kanji: "雨四光",
    group: "brights",
    points: PROFILE.ameShiko,
    counting: false,
    description: "Four brights including the Rainman (11-B).",
    cards: ["01-B", "03-B", "08-B", "11-B"],
  },
  {
    id: "sanko",
    name: "Sanko",
    kanji: "三光",
    group: "brights",
    points: PROFILE.sanko,
    counting: false,
    description: "Three brights without the Rainman.",
    cards: ["01-B", "03-B", "08-B"],
  },
  {
    id: "hanami",
    name: "Hanami-zake",
    kanji: "花見酒",
    group: "fixed",
    points: PROFILE.fixedSet,
    counting: false,
    description: "Cherry-blossom viewing: the Curtain (03-B) plus the Sake Cup (09-A).",
    cards: ["03-B", "09-A"],
  },
  {
    id: "tsukimi",
    name: "Tsukimi-zake",
    kanji: "月見酒",
    group: "fixed",
    points: PROFILE.fixedSet,
    counting: false,
    description: "Moon viewing: the Moon (08-B) plus the Sake Cup (09-A).",
    cards: ["08-B", "09-A"],
  },
  {
    id: "ino-shika-cho",
    name: "Ino-Shika-Chō",
    kanji: "猪鹿蝶",
    group: "fixed",
    points: PROFILE.fixedSet,
    counting: false,
    description: "Boar, deer and butterflies – the three famous animals (06-A, 07-A, 10-A).",
    cards: ["06-A", "07-A", "10-A"],
  },
  {
    id: "akatan",
    name: "Akatan",
    kanji: "赤短",
    group: "ribbons",
    points: PROFILE.fixedSet,
    counting: false,
    description: "The three red poetry ribbons (01-RP, 02-RP, 03-RP).",
    cards: ["01-RP", "02-RP", "03-RP"],
  },
  {
    id: "aotan",
    name: "Aotan",
    kanji: "青短",
    group: "ribbons",
    points: PROFILE.fixedSet,
    counting: false,
    description: "The three blue ribbons (06-RB, 09-RB, 10-RB).",
    cards: ["06-RB", "09-RB", "10-RB"],
  },
  {
    id: "akatan-aotan",
    name: "Akatan + Aotan",
    kanji: "赤短青短",
    group: "ribbons",
    points: 2 * PROFILE.fixedSet,
    counting: false,
    description: "Both ribbon sets together – replaces the two individual yaku with one 10-point yaku.",
    cards: ["01-RP", "02-RP", "03-RP", "06-RB", "09-RB", "10-RB"],
  },
  {
    id: "tane",
    name: "Tane",
    kanji: "タネ",
    group: "counting",
    points: PROFILE.animalThreshold,
    counting: true,
    description: "Five or more animals – one point per animal beyond four.",
    cards: ["02-A", "04-A", "05-A", "06-A", "07-A"],
  },
  {
    id: "tanzaku",
    name: "Tanzaku",
    kanji: "短冊",
    group: "counting",
    points: PROFILE.ribbonThreshold,
    counting: true,
    description: "Four or more ribbons – one point per ribbon beyond three.",
    cards: ["01-RP", "04-R", "05-R", "06-RB"],
  },
  {
    id: "kasu",
    name: "Kasu",
    kanji: "カス",
    group: "counting",
    points: PROFILE.plainThreshold,
    counting: true,
    description: "Ten or more plains – one point per plain beyond nine. The Sake Cup counts as one extra plain.",
    cards: ["01-P1", "01-P2", "02-P1", "02-P2", "03-P1"],
  },
];

export function yakuById(id: YakuId): YakuDef {
  return YAKU.find((y) => y.id === id)!;
}

export interface YakuScore {
  brights: number;
  animals: number;
  ribbons: number;
  physicalPlains: number;
  /** Plains including the Sake Cup's phantom plain. */
  kasuUnits: number;
  rain: boolean;
  cup: boolean;
  /** Points per yaku id (0 = not formed). */
  values: Record<YakuId, number>;
  base: number;
}

/** Exact port of ScoreYaku() from src/KoiKoiRules.cpp. */
export function scoreYaku(captured: string[]): YakuScore {
  const set = new Set(captured);
  const has = (id: string) => set.has(id);
  let brights = 0;
  let animals = 0;
  let ribbons = 0;
  let physicalPlains = 0;
  for (const id of captured) {
    const card = cardById(id);
    if (!card) continue;
    switch (card.type) {
      case "hikari":
        brights++;
        break;
      case "tane":
        animals++;
        break;
      case "tanzaku":
        ribbons++;
        break;
      case "kasu":
        physicalPlains++;
        break;
    }
  }
  const rain = has("11-B");
  const cup = has("09-A");
  const values: Record<YakuId, number> = {
    goko: 0,
    shiko: 0,
    "ame-shiko": 0,
    sanko: 0,
    hanami: 0,
    tsukimi: 0,
    "ino-shika-cho": 0,
    akatan: 0,
    aotan: 0,
    "akatan-aotan": 0,
    tane: 0,
    tanzaku: 0,
    kasu: 0,
  };
  if (brights === 5) values.goko = PROFILE.goko;
  else if (brights === 4) {
    if (rain) values["ame-shiko"] = PROFILE.ameShiko;
    else values.shiko = PROFILE.shiko;
  } else if (brights === 3 && !rain) values.sanko = PROFILE.sanko;

  values.hanami = has("03-B") && cup ? PROFILE.fixedSet : 0;
  values.tsukimi = has("08-B") && cup ? PROFILE.fixedSet : 0;
  values["ino-shika-cho"] = has("06-A") && has("07-A") && has("10-A") ? PROFILE.fixedSet : 0;

  const hasAkatan = has("01-RP") && has("02-RP") && has("03-RP");
  const hasAotan = has("06-RB") && has("09-RB") && has("10-RB");
  if (hasAkatan && hasAotan) values["akatan-aotan"] = 2 * PROFILE.fixedSet;
  else if (hasAkatan) values.akatan = PROFILE.fixedSet;
  else if (hasAotan) values.aotan = PROFILE.fixedSet;

  values.tane = Math.max(0, animals - (PROFILE.animalThreshold - 1));
  values.tanzaku = Math.max(0, ribbons - (PROFILE.ribbonThreshold - 1));
  const kasuUnits = physicalPlains + (cup ? 1 : 0);
  values.kasu = Math.max(0, kasuUnits - (PROFILE.plainThreshold - 1));

  const base = Object.values(values).reduce((a, b) => a + b, 0);
  return { brights, animals, ribbons, physicalPlains, kasuUnits, rain, cup, values, base };
}

export interface Settlement {
  base: number;
  sevenFactor: number;
  counterFactor: number;
  callerFactor: number;
  award: number;
}

/** Exact port of CalculateSettlement() from src/KoiKoiRules.cpp. */
export function settle(base: number, opponentCalledKoi: boolean, selfKoiCalls: number): Settlement {
  const sevenFactor = base >= PROFILE.sevenThreshold ? 2 : 1;
  const counterFactor = opponentCalledKoi ? 2 : 1;
  const callerFactor = 1 + Math.min(selfKoiCalls, PROFILE.maxCallerFactor - 1);
  return { base, sevenFactor, counterFactor, callerFactor, award: base * sevenFactor * counterFactor * callerFactor };
}

/** Which yaku a single card can contribute to – used in the card modal. */
export function yakuForCard(stableId: string): YakuId[] {
  const card = cardById(stableId);
  if (!card) return [];
  const out: YakuId[] = [];
  if (card.type === "hikari") {
    out.push("goko");
    if (stableId === "11-B") out.push("ame-shiko");
    else out.push("shiko", "sanko");
  }
  if (stableId === "03-B") out.push("hanami");
  if (stableId === "08-B") out.push("tsukimi");
  if (stableId === "09-A") out.push("hanami", "tsukimi");
  if (["06-A", "07-A", "10-A"].includes(stableId)) out.push("ino-shika-cho");
  if (["01-RP", "02-RP", "03-RP"].includes(stableId)) out.push("akatan", "akatan-aotan");
  if (["06-RB", "09-RB", "10-RB"].includes(stableId)) out.push("aotan", "akatan-aotan");
  if (card.type === "tane") out.push("tane");
  if (card.type === "tanzaku") out.push("tanzaku");
  if (card.type === "kasu" || card.sakeCup) out.push("kasu");
  return out;
}

/** Type counts of a selection – for the calculator's counter chips. */
export function typeCounts(captured: string[]): Record<CardType, number> {
  const counts: Record<CardType, number> = { hikari: 0, tane: 0, tanzaku: 0, kasu: 0 };
  for (const id of captured) {
    const card = CARDS.find((c) => c.stableId === id);
    if (card) counts[card.type]++;
  }
  return counts;
}
