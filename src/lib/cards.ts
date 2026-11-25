/**
 * The 48 cards, transcribed 1:1 from src/DeckRegister.hpp (the single source of
 * card metadata). Train motifs and descriptions come from card-gallery/gallery_data.json.
 */

export type CardType = "hikari" | "tane" | "tanzaku" | "kasu";
export type RibbonType = "NONE" | "RED_POETRY" | "BLUE" | "RED_PLAIN";

export interface Card {
  /** Numeric id, month-major: (month - 1) * 4 + index. */
  id: number;
  month: number;
  /** Atlas row, 0–3. */
  index: number;
  type: CardType;
  ribbon: RibbonType;
  sakeCup: boolean;
  name: string;
  stableId: string;
  train: string;
  description: string;
}

export const CARDS: Card[] = [
  { id: 0, month: 1, index: 0, type: "hikari", ribbon: "NONE", sakeCup: false, name: "Pine and Crane", stableId: "01-B", train: "JR East E231-500 Series (Yamanote Line)", description: "A red-crowned crane glides beneath twisted pine boughs and a bold red sun while a lime-green Yamanote train rounds the curve toward Shinagawa." },
  { id: 1, month: 1, index: 1, type: "tanzaku", ribbon: "RED_POETRY", sakeCup: false, name: "Pine Poetry Ribbon", stableId: "01-RP", train: "JR East E231-500 Series (Yamanote Line)", description: "A vermilion poem slip sways from old pine needles in the breeze as an afternoon Yamanote commuter train glides past apartment blocks." },
  { id: 2, month: 1, index: 2, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Pine Plain 1", stableId: "01-P1", train: "JR East E231-500 Series (Yamanote Line)", description: "Twisted pine needles arch above double tracks and concrete walls as a Yamanote rapid-display train hums steadily toward Shinagawa." },
  { id: 3, month: 1, index: 3, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Pine Plain 2", stableId: "01-P2", train: "JR East E231-500 Series (Yamanote Line)", description: "Pixelated afternoon clouds drift behind a sturdy pine trunk while a lime-green Yamanote train banks smoothly along Tokyo's inner loop." },
  { id: 4, month: 2, index: 0, type: "tane", ribbon: "NONE", sakeCup: false, name: "Plum and Bush Warbler", stableId: "02-A", train: "JR East 201 Series (Chuo Line Rapid)", description: "A quiet uguisu perches among crimson plum blossoms while a vermilion 201 Series commuter train pulls past a green home signal." },
  { id: 5, month: 2, index: 1, type: "tanzaku", ribbon: "RED_POETRY", sakeCup: false, name: "Plum Poetry Ribbon", stableId: "02-RP", train: "JR East 201 Series (Chuo Line)", description: "Cursive ink dances down a red tanzaku tied to blooming plum twigs as an orange 201 Series commuter train coasts into the station." },
  { id: 6, month: 2, index: 2, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Plum Plain 1", stableId: "02-P1", train: "None (scenic card)", description: "Red plum blossoms reach across quiet station tracks beneath a concrete viaduct, waiting under a clear green departure signal." },
  { id: 7, month: 2, index: 3, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Plum Plain 2", stableId: "02-P2", train: "JR East E231 Series (Shonan-Shinjuku Line)", description: "Seen through a lattice of early spring plum blossoms, a green-and-orange E231 local train eases around the bend toward Shin-Kawasaki." },
  { id: 8, month: 3, index: 0, type: "hikari", ribbon: "NONE", sakeCup: false, name: "Cherry Blossom Curtain", stableId: "03-B", train: "Iyotetsu Moha 50 Series Streetcar", description: "Winking in crisp navy uniform, a conductor salutes on the tracks in front of a festival maku curtain, cherry trees, and a vintage tram." },
  { id: 9, month: 3, index: 1, type: "tanzaku", ribbon: "RED_POETRY", sakeCup: false, name: "Cherry Poetry Ribbon", stableId: "03-RP", train: "Iyotetsu Retro Streetcar (Otemachi Line)", description: "A bright red poetry ribbon sways over cobblestone tram tracks while an orange streetcar bound for Otemachi rolls past neighborhood shops." },
  { id: 10, month: 3, index: 2, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Cherry Plain 1", stableId: "03-P1", train: "None (scenic card)", description: "Pink petals drift across empty gravel-ballasted tracks under bright blue skies, guarded by a glowing red stop signal and brass master controller." },
  { id: 11, month: 3, index: 3, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Cherry Plain 2", stableId: "03-P2", train: "Iyotetsu Retro Streetcar (Otemachi Line)", description: "Viewed through a tunnel of spring sakura, a nostalgic orange tram trundles down a paved city avenue with distant mountains behind." },
  { id: 12, month: 4, index: 0, type: "tane", ribbon: "NONE", sakeCup: false, name: "Wisteria and Cuckoo", stableId: "04-A", train: "JR Kyushu 813 Series (Kagoshima Main Line)", description: "Beneath a full moon and cascading purple wisteria, a night-flying hototogisu crosses the path of a red 813 Series train." },
  { id: 13, month: 4, index: 1, type: "tanzaku", ribbon: "RED_PLAIN", sakeCup: false, name: "Wisteria Red Ribbon", stableId: "04-R", train: "JR Kyushu 813 Series (Kagoshima Main Line)", description: "Suspended from dark wisteria branches by braided cord, a crimson tanzaku sways above the distant headlights of a night Kagoshima Line train." },
  { id: 14, month: 4, index: 2, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Wisteria Plain 1", stableId: "04-P1", train: "JR Kyushu 813 Series (Kagoshima Main Line)", description: "Thick curtains of purple wisteria hang from steel catenary beams as a crimson JR Kyushu commuter train slips past warm house windows." },
  { id: 15, month: 4, index: 3, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Wisteria Plain 2", stableId: "04-P2", train: "None (scenic card)", description: "Headlight beams illuminate empty wooden sleepers under hanging wisteria and a quiet full moon, with four stations left on the night run." },
  { id: 16, month: 5, index: 0, type: "tane", ribbon: "NONE", sakeCup: false, name: "Iris and Bridge", stableId: "05-A", train: "Akita Shinkansen E3 Series (Komachi)", description: "Standing on a rustic yatsuhashi plank bridge amid blooming irises, an upbeat conductor waves as the magenta-nosed Komachi speeds through Akita." },
  { id: 17, month: 5, index: 1, type: "tanzaku", ribbon: "RED_PLAIN", sakeCup: false, name: "Iris Red Ribbon", stableId: "05-R", train: "Akita Shinkansen E3 Series (Komachi)", description: "Tied with a neat lucky knot, a bright red ribbon hangs over straight ballast tracks as an E3 Komachi bullet train approaches from afar." },
  { id: 18, month: 5, index: 2, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Iris Plain 1", stableId: "05-P1", train: "Akita Shinkansen E3 Series (Komachi)", description: "Crisp low-poly purple irises bloom along a clear trackside waterway while a pink-and-white E3 Komachi races between Akita and Morioka." },
  { id: 19, month: 5, index: 3, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Iris Plain 2", stableId: "05-P2", train: "JR East E2 Series Shinkansen", description: "Endless fields of purple water irises stretch to the horizon as a sleek white-and-pink Shinkansen glides swiftly across the flooded plain." },
  { id: 20, month: 6, index: 0, type: "tane", ribbon: "NONE", sakeCup: false, name: "Peony and Butterflies", stableId: "06-A", train: "Tokaido/Sanyo Shinkansen 300 Series", description: "Framed like a railfan viewfinder shot, two yellow butterflies flutter around lush red peonies while a wedge-nosed Shinkansen glides past." },
  { id: 21, month: 6, index: 1, type: "tanzaku", ribbon: "BLUE", sakeCup: false, name: "Peony Blue Ribbon", stableId: "06-RB", train: "Sanyo Shinkansen 700 Series (Hikari Rail Star)", description: "A deep blue tanzaku hangs beside red peonies and catenary wires as a sleek Shinkansen bearing Rail Star lettering rounds the urban curve." },
  { id: 22, month: 6, index: 2, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Peony Plain 1", stableId: "06-P1", train: "Sanyo Shinkansen 700 Series", description: "Crimson pixel peonies brighten the narrow green divider between urban tracks as a duck-billed white-and-blue Shinkansen cruises into view." },
  { id: 23, month: 6, index: 3, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Peony Plain 2", stableId: "06-P2", train: "Sanyo Shinkansen 700 Series", description: "Lush summer peonies bloom against a trackside fence while an aerodynamic white-and-blue Shinkansen sweeps quietly past neighborhood homes." },
  { id: 24, month: 7, index: 0, type: "tane", ribbon: "NONE", sakeCup: false, name: "Clover and Boar", stableId: "07-A", train: "JR East E231 Series (Shonan-Shinjuku Line)", description: "Among arching pink hagi blossoms by the fence, a woman in blue-and-white pets a friendly polygonal boar while an E231 local waits at the platform." },
  { id: 25, month: 7, index: 1, type: "tanzaku", ribbon: "RED_PLAIN", sakeCup: false, name: "Clover Red Ribbon", stableId: "07-R", train: "JR East E231 Series (Shonan-Shinjuku Line)", description: "Tied with braided red-and-white cord among blooming hagi, a crimson tanzaku catches the breeze as a Shonan-Shinjuku train rolls past." },
  { id: 26, month: 7, index: 2, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Clover Plain 1", stableId: "07-P1", train: "JR East E231 Series (Shonan-Shinjuku Line)", description: "Clusters of magenta bush clover lean over concrete fencing under a green signal as a Shonan-Shinjuku commuter train glides along the platform." },
  { id: 27, month: 7, index: 3, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Clover Plain 2", stableId: "07-P2", train: "JR East E231 Series (Shonan-Shinjuku Line)", description: "Delicate pink hagi branches sway beside white concrete posts while a green-and-orange E231 commuter train eases under the station footbridge." },
  { id: 28, month: 8, index: 0, type: "hikari", ribbon: "NONE", sakeCup: false, name: "Pampas Grass and Moon", stableId: "08-B", train: "JR West 500 Series Shinkansen", description: "Under a luminous full moon near Shin-Osaka, a woman in a crimson jacket smiles beside silver susuki plumes as a 500 Series rockets along the viaduct." },
  { id: 29, month: 8, index: 1, type: "tane", ribbon: "NONE", sakeCup: false, name: "Pampas Grass and Geese", stableId: "08-A", train: "JR West 500 Series Shinkansen", description: "Three wild geese wing across a violet evening sky while a jet-like blue 500 Series bullet train streaks at 213 km/h toward Shin-Kobe." },
  { id: 30, month: 8, index: 2, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Pampas Grass Plain 1", stableId: "08-P1", train: "JR West 500 Series Shinkansen", description: "Feathery pampas grass ripples on a steep green hillside under overcast skies while a blue 500 Series Shinkansen glides toward Shin-Kobe." },
  { id: 31, month: 8, index: 3, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Pampas Grass Plain 2", stableId: "08-P2", train: "JR West 500 Series Shinkansen", description: "Silver susuki tassels sway above hillside rooftops as JR West's needle-nosed 500 Series sweeps gracefully around a mountain-backed viaduct." },
  { id: 32, month: 9, index: 0, type: "tane", ribbon: "NONE", sakeCup: true, name: "Chrysanthemum and Sake Cup", stableId: "09-A", train: "Tokaido/Sanyo Shinkansen 0 Series", description: "A ceramic seishu flask and porcelain cup rest among red and gold chrysanthemums on a rooftop wall as an iconic round-nosed 0 Series glides past." },
  { id: 33, month: 9, index: 1, type: "tanzaku", ribbon: "BLUE", sakeCup: false, name: "Chrysanthemum Blue Ribbon", stableId: "09-RB", train: "Tokaido/Sanyo Shinkansen 0 Series", description: "Suspended from knotted twine over lush autumn chrysanthemums, an indigo ribbon frames a classic 0 Series Shinkansen cruising overhead." },
  { id: 34, month: 9, index: 2, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Chrysanthemum Plain 1", stableId: "09-P1", train: "Tokaido/Sanyo Shinkansen 100 Series", description: "Sunny yellow and crimson chrysanthemums line the elevated trackside fence beneath clear autumn skies as a shark-nosed Shinkansen approaches." },
  { id: 35, month: 9, index: 3, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Chrysanthemum Plain 2", stableId: "09-P2", train: "Tokaido/Sanyo Shinkansen 0 Series", description: "Headlights shining at 169 km/h, a nostalgic white-and-blue 0 Series Shinkansen hums along an urban viaduct bordered by autumn kiku." },
  { id: 36, month: 10, index: 0, type: "tane", ribbon: "NONE", sakeCup: false, name: "Maple and Deer", stableId: "10-A", train: "JR West 207 Series", description: "Antlers raised calmly, a polygonal buck stands on leaf-strewn grass beneath autumn maples as a silver-and-blue JR West train rolls past." },
  { id: 37, month: 10, index: 1, type: "tanzaku", ribbon: "BLUE", sakeCup: false, name: "Maple Blue Ribbon", stableId: "10-RB", train: "JR West 221 Series", description: "Surrounded by angular red and amber maple leaves, a cool blue tanzaku sways above the rails while a commuter train glides toward the station." },
  { id: 38, month: 10, index: 2, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Maple Plain 1", stableId: "10-P1", train: "JR West 221 Series", description: "Scarlet momiji leaves hang over steel catenary arches and fallen petals on the ballast while a white JR West rapid train coasts forward." },
  { id: 39, month: 10, index: 3, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Maple Plain 2", stableId: "10-P2", train: "JR West 221 Series", description: "Dusk settles with retro dithering over city rooftops and a glowing autumn maple tree as a commuter train rounds the elevated curve." },
  { id: 40, month: 11, index: 0, type: "hikari", ribbon: "NONE", sakeCup: false, name: "Willow and Rainman", stableId: "11-B", train: "Sanyo Shinkansen 300 Series", description: "Stepping across rain-slick river stones under a traditional painted umbrella, a calm station attendant watches a 300 Series pass willow boughs." },
  { id: 41, month: 11, index: 1, type: "tane", ribbon: "NONE", sakeCup: false, name: "Willow and Swallow", stableId: "11-A", train: "Sanyo Shinkansen 300 Series", description: "Streaks of November rain fall across weeping willow branches while a swallow darts over wet tracks ahead of a passing Shinkansen." },
  { id: 42, month: 11, index: 2, type: "tanzaku", ribbon: "RED_PLAIN", sakeCup: false, name: "Willow Red Ribbon", stableId: "11-R", train: "Sanyo Shinkansen 300 Series", description: "Rain patters onto station roofs and trailing green willow leaves while a dark crimson tanzaku hangs beside a gliding bullet train." },
  { id: 43, month: 11, index: 3, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Willow and Lightning", stableId: "11-P1", train: "Sanyo Shinkansen 300 Series", description: "Dark storm clouds gather over a rain-rippled trackside canal and drooping willows as a white-and-blue bullet train eases around the platform." },
  { id: 44, month: 12, index: 0, type: "hikari", ribbon: "NONE", sakeCup: false, name: "Paulownia and Phoenix", stableId: "12-B", train: "JR Kyushu 883 Series (Sonic)", description: "Golden tail feathers trailing like sparks, a mythical phoenix soars over purple paulownia blossoms as a blue 883 Sonic hums through the night." },
  { id: 45, month: 12, index: 1, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Paulownia Plain 1", stableId: "12-P1", train: "JR Kyushu 883 Series (Sonic)", description: "Bright quad headlights pierce the evening haze as a metallic-blue JR Kyushu Sonic glides past purple paulownia blooms and quiet houses." },
  { id: 46, month: 12, index: 2, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Paulownia Plain 2", stableId: "12-P2", train: "JR Kyushu 883 Series (Sonic)", description: "Under a warm trackside streetlamp, broad paulownia leaves and violet spires frame a blue 883 Series express rounding the night curve." },
  { id: 47, month: 12, index: 3, type: "kasu", ribbon: "NONE", sakeCup: false, name: "Paulownia Plain 3", stableId: "12-P3", train: "JR Kyushu 883 Series (Sonic)", description: "Tall stalks of purple kiri blossoms stand in the cool December night while a blue Kyushu express rolls softly past glowing house windows." },
];

export interface Month {
  n: number;
  name: string;
  kanji: string;
  romaji: string;
  plant: string;
}

export const MONTHS: Month[] = [
  { n: 1, name: "January", kanji: "松", romaji: "Matsu", plant: "Pine" },
  { n: 2, name: "February", kanji: "梅", romaji: "Ume", plant: "Plum Blossom" },
  { n: 3, name: "March", kanji: "桜", romaji: "Sakura", plant: "Cherry Blossom" },
  { n: 4, name: "April", kanji: "藤", romaji: "Fuji", plant: "Wisteria" },
  { n: 5, name: "May", kanji: "菖蒲", romaji: "Ayame", plant: "Iris" },
  { n: 6, name: "June", kanji: "牡丹", romaji: "Botan", plant: "Peony" },
  { n: 7, name: "July", kanji: "萩", romaji: "Hagi", plant: "Bush Clover" },
  { n: 8, name: "August", kanji: "薄", romaji: "Susuki", plant: "Pampas Grass" },
  { n: 9, name: "September", kanji: "菊", romaji: "Kiku", plant: "Chrysanthemum" },
  { n: 10, name: "October", kanji: "紅葉", romaji: "Momiji", plant: "Maple" },
  { n: 11, name: "November", kanji: "柳", romaji: "Yanagi", plant: "Willow" },
  { n: 12, name: "December", kanji: "桐", romaji: "Kiri", plant: "Paulownia" },
];

export interface TypeInfo {
  type: CardType;
  label: string;
  japanese: string;
  english: string;
  count: number;
  /** Tailwind-friendly accent name used across the wiki. */
  accent: "amber" | "emerald" | "rose" | "slate";
  blurb: string;
}

export const TYPE_INFO: Record<CardType, TypeInfo> = {
  hikari: {
    type: "hikari",
    label: "Hikari",
    japanese: "光",
    english: "Brights",
    count: 5,
    accent: "amber",
    blurb: "The five showpiece cards – crane, curtain, moon, rainman and phoenix.",
  },
  tane: {
    type: "tane",
    label: "Tane",
    japanese: "タネ",
    english: "Animals",
    count: 9,
    accent: "emerald",
    blurb: "Animals and objects – boar, deer, butterflies, the sake cup and more.",
  },
  tanzaku: {
    type: "tanzaku",
    label: "Tanzaku",
    japanese: "短冊",
    english: "Ribbons",
    count: 10,
    accent: "rose",
    blurb: "The paper strips – three poetry ribbons, three blue, four plain red.",
  },
  kasu: {
    type: "kasu",
    label: "Kasu",
    japanese: "カス",
    english: "Plains",
    count: 24,
    accent: "slate",
    blurb: "The plain plant cards – the quiet majority of the deck.",
  },
};

export const TYPE_ORDER: CardType[] = ["hikari", "tane", "tanzaku", "kasu"];

export function cardById(stableId: string): Card | undefined {
  return CARDS.find((c) => c.stableId === stableId);
}

export function cardsOfMonth(month: number): Card[] {
  return CARDS.filter((c) => c.month === month);
}

export function monthOf(n: number): Month {
  return MONTHS[n - 1];
}

/* Embedded 200 px previews (bundled into the single-file build). */
const previews = import.meta.glob("../assets/cards/*.jpg", {
  eager: true,
  query: "?url",
  import: "default",
}) as Record<string, string>;

export function cardPreview(stableId: string): string {
  return previews[`../assets/cards/${stableId}.jpg`];
}

/* Full-resolution originals (602×928), loaded on demand in the card modal. */
export const CARD_FULL_BASE =
  "https://raw.githubusercontent.com/Aquaslopper/DenshaDeKoi/main/assets/options/card-gallery/cards/";

export function cardFullUrl(stableId: string): string {
  return `${CARD_FULL_BASE}${stableId}.jpg`;
}

export const REPO_URL = "https://github.com/Aquaslopper/DenshaDeKoi";
export const RELEASES_URL = "https://github.com/Aquaslopper/DenshaDeKoi/releases";
export const ACTIONS_URL = "https://github.com/Aquaslopper/DenshaDeKoi/actions";
export const GALLERY_URL = "https://github.com/Aquaslopper/DenshaDeKoi/tree/main/card-gallery";
