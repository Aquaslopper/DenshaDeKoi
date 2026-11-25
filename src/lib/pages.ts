import {
  BookOpen,
  CircleHelp,
  Code2,
  Download,
  Gamepad2,
  GraduationCap,
  Home,
  Layers,
  Library,
  Settings2,
  Sparkles,
  Trophy,
  type LucideIcon,
} from "lucide-react";

export type Level = "Beginner" | "Advanced" | "Developer";

export interface PageMeta {
  id: string;
  nav: string;
  group: string;
  level: Level;
  icon: LucideIcon;
  keywords: string[];
  lead: string;
}

export const GROUPS = ["First steps", "Playing", "Menus & extras", "Reference"] as const;

export const PAGES: PageMeta[] = [
  {
    id: "start",
    nav: "Home",
    group: "First steps",
    level: "Beginner",
    icon: Home,
    keywords: ["home", "welcome", "overview", "start", "densha de koi", "train", "hanafuda", "intro"],
    lead: "A beginner-friendly guide to the train-themed Koi-Koi game.",
  },
  {
    id: "installation",
    nav: "Installation",
    group: "First steps",
    level: "Beginner",
    icon: Download,
    keywords: ["install", "build", "cmake", "windows", "macos", "linux", "download", "release", "artifact", "raylib", "compile"],
    lead: "From zero to a running game in four commands – or grab a prebuilt Windows download.",
  },
  {
    id: "grundlagen",
    nav: "Learn Koi-Koi",
    group: "First steps",
    level: "Beginner",
    icon: GraduationCap,
    keywords: ["learn", "basics", "tutorial", "hanafuda", "rules", "beginner", "how to play", "turn", "deal", "quiz", "month"],
    lead: "Never played Hanafuda? Perfect. This page starts at zero and explains everything.",
  },
  {
    id: "steuerung",
    nav: "Controls",
    group: "Playing",
    level: "Beginner",
    icon: Gamepad2,
    keywords: ["controls", "mouse", "drag", "drop", "keyboard", "esc", "pause", "input", "hud", "round", "result", "ledger"],
    lead: "Every mouse move and key you need – from the start menu to reading the match result.",
  },
  {
    id: "yaku",
    nav: "Yaku & Scoring",
    group: "Playing",
    level: "Beginner",
    icon: Trophy,
    keywords: ["yaku", "scoring", "points", "goko", "shiko", "sanko", "hanami", "tsukimi", "ino shika cho", "akatan", "aotan", "tane", "tanzaku", "kasu", "calculator", "multiplier"],
    lead: "Every winning combination and every point in the game – with an interactive calculator.",
  },
  {
    id: "karten",
    nav: "All 48 Cards",
    group: "Playing",
    level: "Beginner",
    icon: Layers,
    keywords: ["cards", "deck", "gallery", "month", "pine", "crane", "sake cup", "rainman", "phoenix", "bright", "ribbon", "plain", "animal", "id"],
    lead: "The complete railway-themed deck, month by month – click any card for details.",
  },
  {
    id: "optionen",
    nav: "Menus & Options",
    group: "Menus & extras",
    level: "Beginner",
    icon: Settings2,
    keywords: ["options", "settings", "pause", "menu", "volume", "hints", "resolution", "fullscreen", "anti-aliasing", "rounds", "save"],
    lead: "The start menu, match settings and the six pause-menu options, explained one by one.",
  },
  {
    id: "extras",
    nav: "Gallery, Radio, Sound",
    group: "Menus & extras",
    level: "Beginner",
    icon: Sparkles,
    keywords: ["gallery", "3d", "viewer", "radio", "stream", "j-pop", "asia dream radio", "sound", "sfx", "wav", "music", "zoom"],
    lead: "The 3D card viewer, six J-Pop radio stations and the optional sound-effect bank.",
  },
  {
    id: "regelwerk",
    nav: "Rules in Detail",
    group: "Reference",
    level: "Advanced",
    icon: BookOpen,
    keywords: ["rules", "kk-add-10", "profile", "constants", "dealer", "misdeal", "teshi", "kuttsuki", "lucky hand", "settlement", "state machine", "xeno-koi"],
    lead: "The exact KK-ADD-10 ruleset as implemented in the source code – for the curious.",
  },
  {
    id: "glossar",
    nav: "Glossary",
    group: "Reference",
    level: "Beginner",
    icon: Library,
    keywords: ["glossary", "terms", "definitions", "japanese", "vocabulary", "dictionary"],
    lead: "Every Japanese and game term on one page – searchable and filterable.",
  },
  {
    id: "faq",
    nav: "FAQ & Help",
    group: "Reference",
    level: "Beginner",
    icon: CircleHelp,
    keywords: ["faq", "help", "questions", "problems", "troubleshooting", "radio", "sound", "start", "browser"],
    lead: "Quick answers to the questions that come up most – with direct links.",
  },
  {
    id: "entwicklung",
    nav: "Development",
    group: "Reference",
    level: "Developer",
    icon: Code2,
    keywords: ["development", "code", "c++", "raylib", "architecture", "tests", "ci", "contributing", "protected", "state machine", "deck"],
    lead: "How the code is organized, how to test it, and which areas are protected.",
  },
];

export function pageById(id: string): PageMeta | undefined {
  return PAGES.find((p) => p.id === id);
}
