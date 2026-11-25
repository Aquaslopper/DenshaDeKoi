import { slug } from "./slug";

export type GlossaryTag = "Game" | "Cards" | "Yaku" | "Tech";

export interface GlossaryEntry {
  term: string;
  kanji?: string;
  tag: GlossaryTag;
  definition: string;
  /** Optional wiki link, e.g. "yaku/yaku-goko". */
  link?: string;
  linkLabel?: string;
}

const ENTRIES: GlossaryEntry[] = [
  { term: "Akatan", kanji: "赤短", tag: "Yaku", definition: "The three red poetry ribbons (01-RP, 02-RP, 03-RP) – worth 5 points. Together with Aotan it becomes the 10-point Akatan + Aotan.", link: "yaku/yaku-akatan", linkLabel: "See Akatan" },
  { term: "Ame-Shiko", kanji: "雨四光", tag: "Yaku", definition: "Four brights including the Rainman – worth 7 points, one less than Shiko without him.", link: "yaku/yaku-ame-shiko", linkLabel: "See Ame-Shiko" },
  { term: "Anti-Aliasing", tag: "Tech", definition: "A pause-menu option that supersamples the 3D table at 2×, 3× or 4× before the UI is drawn, smoothing card edges.", link: "optionen", linkLabel: "See options" },
  { term: "Atlas", tag: "Tech", definition: "The single packed image (cards_atlas.png) holding all 48 card faces in a 12×4 grid of 179×264 px cells.", link: "entwicklung", linkLabel: "See development" },
  { term: "Aotan", kanji: "青短", tag: "Yaku", definition: "The three blue ribbons (06-RB, 09-RB, 10-RB) – worth 5 points.", link: "yaku/yaku-aotan", linkLabel: "See Aotan" },
  { term: "Bullet time", tag: "Game", definition: "The one-second slow-motion beat at the start of each round: the Koidenscha logo spins above the table while hands and field are dealt.", link: "steuerung", linkLabel: "See controls" },
  { term: "Capture pile", tag: "Game", definition: "The face-up stack beside each player where captured cards are collected. Only your own pile counts toward yaku.", link: "grundlagen", linkLabel: "Learn the basics" },
  { term: "Card Hints", tag: "Game", definition: "A pause-menu option that outlines matching field cards while you hover or drag – recommended for beginners.", link: "optionen", linkLabel: "See options" },
  { term: "CMake", tag: "Tech", definition: "The build system (version 3.24 or newer) that configures the project and fetches Raylib 5.5 automatically.", link: "installation", linkLabel: "See installation" },
  { term: "CPU opponent", tag: "Game", definition: "The computer player. It currently uses a simple first-available-match strategy and always banks a yaku instead of calling Koi-Koi.", link: "faq/faq-cpu", linkLabel: "See FAQ" },
  { term: "Dealer (Oya)", kanji: "親", tag: "Game", definition: "The player who acts first in a round. Chosen by an opening card draw; the previous round's winner deals next, a draw keeps the dealer.", link: "regelwerk", linkLabel: "See rules" },
  { term: "Field", tag: "Game", definition: "The eight face-up cards in the middle of the table. You match hand and stock cards against them by month.", link: "grundlagen", linkLabel: "Learn the basics" },
  { term: "Goko", kanji: "五光", tag: "Yaku", definition: "All five brights – the strongest yaku, worth 10 base points.", link: "yaku/yaku-goko", linkLabel: "See Goko" },
  { term: "Hanafuda", kanji: "花札", tag: "Cards", definition: "The traditional Japanese 48-card deck: 12 months with 4 cards each, no numbers. Densha de Koi reimagines every motif with trains.", link: "karten", linkLabel: "See all cards" },
  { term: "Hanami-zake", kanji: "花見酒", tag: "Yaku", definition: "“Flower-viewing sake”: the Cherry Blossom Curtain (03-B) plus the Sake Cup (09-A) – 5 points.", link: "yaku/yaku-hanami", linkLabel: "See Hanami-zake" },
  { term: "Hand", tag: "Game", definition: "The eight cards dealt to each player. You play them one per turn by dragging them onto the field.", link: "steuerung", linkLabel: "See controls" },
  { term: "Hikari", kanji: "光", tag: "Cards", definition: "The five brights – the most valuable cards: Crane, Curtain, Moon, Rainman and Phoenix.", link: "yaku", linkLabel: "See bright yaku" },
  { term: "Ino-Shika-Chō", kanji: "猪鹿蝶", tag: "Yaku", definition: "Boar (07-A), Deer (10-A) and Butterflies (06-A) – 5 points, and it stacks with the Tane counting yaku.", link: "yaku/yaku-ino-shika-cho", linkLabel: "See Ino-Shika-Chō" },
  { term: "Kasu", kanji: "カス", tag: "Cards", definition: "The 24 plain cards. Ten or more score the Kasu yaku; the Sake Cup counts as one extra plain.", link: "yaku/yaku-kasu", linkLabel: "See Kasu" },
  { term: "KK-ADD-10", tag: "Game", definition: "The one ruleset Densha de Koi implements, taken from Xeno-koi: additive yaku, a 10-point combined ribbon set and a ×2 award from 7 base points.", link: "regelwerk", linkLabel: "See rules in detail" },
  { term: "Koi-Koi", kanji: "こいこい", tag: "Game", definition: "“Come on, come on!” – the call that declines banking and keeps the round going for more points. Each own call raises your award factor (up to ×6), but an opponent win after your call doubles their award.", link: "grundlagen", linkLabel: "Learn the basics" },
  { term: "Kuttsuki", kanji: "くっつき", tag: "Game", definition: "A lucky opening hand of four pairs (two cards of each of four months) – an instant 6-point round win.", link: "regelwerk", linkLabel: "See lucky hands" },
  { term: "Lucky hand", tag: "Game", definition: "Teshi or Kuttsuki dealt right into your hand: the round ends immediately for a flat 6 points. If both players are lucky, the round is a draw.", link: "regelwerk", linkLabel: "See lucky hands" },
  { term: "Misdeal", tag: "Game", definition: "A field showing all four cards of one month, or four pairs. The round is redealt automatically.", link: "regelwerk", linkLabel: "See misdeals" },
  { term: "Month", tag: "Cards", definition: "The “suit” of Hanafuda: each of the 12 months has a plant and exactly four cards. Only cards of the same month match.", link: "karten", linkLabel: "See all cards" },
  { term: "Poetry ribbon", tag: "Cards", definition: "A red ribbon with writing on it (RP). The three poetry ribbons of January–March form the Akatan yaku.", link: "yaku/yaku-akatan", linkLabel: "See Akatan" },
  { term: "Rainman", tag: "Cards", definition: "The November bright “Willow and Rainman” (11-B). He spoils Sanko and Shiko: three brights with him score nothing, four with him score Ame-Shiko (7).", link: "karten", linkLabel: "See the card" },
  { term: "Raylib", tag: "Tech", definition: "The C game-programming library (version 5.5) used for rendering, audio and input. CMake fetches it automatically.", link: "installation", linkLabel: "See installation" },
  { term: "Round", tag: "Game", definition: "One deal of the deck, played until Shobu, a lucky hand or both hands run empty. A standard match lasts three rounds.", link: "grundlagen", linkLabel: "Learn the basics" },
  { term: "Sake Cup", kanji: "盃", tag: "Cards", definition: "The September animal “Chrysanthemum and Sake Cup” (09-A). It counts as a Tane and as one phantom plain, and completes both viewing yaku.", link: "yaku", linkLabel: "See yaku" },
  { term: "Sanko", kanji: "三光", tag: "Yaku", definition: "Three brights without the Rainman – 5 points.", link: "yaku/yaku-sanko", linkLabel: "See Sanko" },
  { term: "Shiko", kanji: "四光", tag: "Yaku", definition: "Four brights without the Rainman – 8 points.", link: "yaku/yaku-shiko", linkLabel: "See Shiko" },
  { term: "Shobu", kanji: "勝負", tag: "Game", definition: "“Settle it!” – the call that banks your current yaku and ends the round. Spelled Shoubu in the source code.", link: "grundlagen", linkLabel: "Learn the basics" },
  { term: "Stable ID", tag: "Tech", definition: "The frozen card identifier such as 01-B or 09-A: two-digit month plus a type letter. It never changes, unlike array positions.", link: "karten", linkLabel: "See card IDs" },
  { term: "Stock", tag: "Game", definition: "The 24 face-down cards left after the deal. One flips automatically after every hand card and resolves the same way.", link: "steuerung", linkLabel: "See controls" },
  { term: "Tane", kanji: "タネ", tag: "Cards", definition: "The nine animal/object cards. Five or more score the Tane yaku, +1 per extra card.", link: "yaku/yaku-tane", linkLabel: "See Tane" },
  { term: "Tanzaku", kanji: "短冊", tag: "Cards", definition: "The ten ribbon cards. Four or more score the Tanzaku yaku, +1 per extra ribbon.", link: "yaku/yaku-tanzaku", linkLabel: "See Tanzaku" },
  { term: "Teshi", kanji: "手四", tag: "Game", definition: "A lucky opening hand with all four cards of one month – an instant 6-point round win.", link: "regelwerk", linkLabel: "See lucky hands" },
  { term: "Tsukimi-zake", kanji: "月見酒", tag: "Yaku", definition: "“Moon-viewing sake”: the Moon (08-B) plus the Sake Cup (09-A) – 5 points.", link: "yaku/yaku-tsukimi", linkLabel: "See Tsukimi-zake" },
  { term: "Xeno-koi", tag: "Tech", definition: "The sibling Koi-Koi project whose KK-ADD-10 rules (commit f422bac) Densha de Koi implements. Its speed minigame and advanced AI are not included here.", link: "regelwerk", linkLabel: "See rules in detail" },
  { term: "Yaku", kanji: "役", tag: "Yaku", definition: "A scoring combination of captured cards – the only source of points in Koi-Koi. All yaku add up.", link: "yaku", linkLabel: "See all yaku" },
];

export interface GlossaryItem extends GlossaryEntry {
  slug: string;
}

export const GLOSSARY: GlossaryItem[] = ENTRIES.map((e) => ({ ...e, slug: slug(e.term) })).sort((a, b) =>
  a.term.localeCompare(b.term, "en"),
);

export const GLOSSARY_TAGS: GlossaryTag[] = ["Game", "Cards", "Yaku", "Tech"];
