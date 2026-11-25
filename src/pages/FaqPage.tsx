import { useEffect, useState, type ReactNode } from "react";
import { Link } from "@/lib/router";
import { useRouter } from "@/lib/router";
import { RELEASES_URL } from "@/lib/cards";
import { AccordionItem, CodeBlock, H2, PageHeader } from "@/components/ui";

interface FaqItem {
  id: string;
  q: string;
  a: ReactNode;
}

const GROUPS: { title: string; items: FaqItem[] }[] = [
  {
    title: "Start & installation",
    items: [
      {
        id: "faq-installer",
        q: "Is there an installer?",
        a: (
          <p>
            Not a classic one. The <a href={RELEASES_URL} target="_blank" rel="noreferrer">Releases page</a>{" "}
            offers packaged beta archives (e.g. <code>DENSHA.DE.KOI.v3.7z</code>) – extract and play. Otherwise
            it's four commands with CMake; see <Link to="installation">Installation</Link>.
          </p>
        ),
      },
      {
        id: "faq-systems",
        q: "Which systems does the game run on?",
        a: (
          <p>
            The README documents builds for <strong>Windows, macOS and Linux</strong> (settings paths exist for
            all three). The CI currently builds and tests Windows binaries with MSVC and MinGW-w64. You need
            CMake ≥ 3.24, a C++20 compiler and Git; Raylib 5.5 is fetched automatically.
          </p>
        ),
      },
      {
        id: "faq-windows-build",
        q: "Where do I get a ready-made Windows version?",
        a: (
          <p>
            Two places: the <a href={RELEASES_URL} target="_blank" rel="noreferrer">Releases page</a> (no login
            needed), or the <Link to="installation">CI artifacts</Link> – the{" "}
            <code>denshadekoi-windows-msvc</code> artifact of the latest successful Build run under Actions
            (GitHub login required for artifacts).
          </p>
        ),
      },
      {
        id: "faq-wont-start",
        q: "The game won't start – what can I check?",
        a: (
          <>
            <p>According to the README, the usual suspects are:</p>
            <p>
              <strong>Assets missing:</strong> the game loads the atlas and textures from the directory beside
              the executable – keep <code>cards_atlas.png</code>, <code>assets/</code> contents and{" "}
              <code>options/</code> together with the .exe. <strong>Wrong directory on Windows:</strong> with
              Visual Studio builds, run <code>build\Debug\denshadekoi.exe</code> (or Release), not the build
              root. <strong>Audio:</strong> the radio needs a working audio device; the console's{" "}
              <code>[radio]</code> lines show where it fails.
            </p>
          </>
        ),
      },
      {
        id: "faq-settings-file",
        q: "Where are my settings saved?",
        a: (
          <p>
            In <code>options.txt</code> inside a <code>DenshaDeKoi</code> folder: <code>%APPDATA%</code> on
            Windows, <code>~/Library/Application Support</code> on macOS, <code>$XDG_CONFIG_HOME</code> or{" "}
            <code>~/.config</code> on Linux. Delete the file to restore defaults. Details in{" "}
            <Link to="optionen">Menus & Options</Link>.
          </p>
        ),
      },
    ],
  },
  {
    title: "Playing",
    items: [
      {
        id: "faq-win-round",
        q: "How do I win a round?",
        a: (
          <p>
            Capture cards into your pile, form a <Link to="yaku">yaku</Link>, and press <strong>Shobu</strong>{" "}
            to bank it – the round ends and you score. Alternatively, a lucky opening hand (Teshi or Kuttsuki)
            wins instantly for 6 points. If both hands run empty without a bank, it's a 0 : 0 draw.
          </p>
        ),
      },
      {
        id: "faq-when-koi",
        q: "When should I call Koi-Koi?",
        a: (
          <p>
            When your hand is strong and you expect to grow the score – each own call raises your factor up to
            ×6. But remember: the CPU <strong>always banks</strong> at its first yaku, and if it wins after your
            call, its award <strong>doubles</strong>. A Koi-Koi call against this CPU is a real gamble. More in{" "}
            <Link to="grundlagen">Learn Koi-Koi</Link>.
          </p>
        ),
      },
      {
        id: "faq-rainman",
        q: "Why didn't my three brights score?",
        a: (
          <p>
            The <strong>Rainman</strong> (11-B) was probably among them. Three brights <em>with</em> him score
            nothing – he only counts toward four brights (Ame-Shiko, 7) or all five (Goko, 10). Without him,
            three brights are Sanko (5). See <Link to="yaku/yaku-ame-shiko">Ame-Shiko</Link>.
          </p>
        ),
      },
      {
        id: "faq-sake-cup",
        q: "Does the sake cup count as a plain?",
        a: (
          <p>
            Yes – sort of! The sake cup (09-A) is an <strong>animal</strong> that <em>also</em> counts as one
            phantom plain. Nine physical plains plus the cup already score Kasu 1. And with the Curtain or the
            Moon it completes the viewing yaku. The most flexible card in the deck.
          </p>
        ),
      },
      {
        id: "faq-cpu",
        q: "Why does the CPU never call Koi-Koi?",
        a: (
          <p>
            Because it's a simple opponent, by design: according to the README it uses a first-available-match
            strategy and <strong>always banks a yaku</strong>. Xeno-koi's advanced AI is not part of
            DenshaDeKoi (currently). That makes it a forgiving first opponent – and a predictable one.
          </p>
        ),
      },
      {
        id: "faq-empty-hands",
        q: "What happens when both hands run empty?",
        a: (
          <p>
            If nobody banked, the round is a <strong>0 : 0 draw</strong> and the dealer stays. One exception: if
            your hand empties right after a new or grown yaku, the game banks it automatically as Shobu – even
            if the CPU still holds a card.
          </p>
        ),
      },
    ],
  },
  {
    title: "Tech",
    items: [
      {
        id: "faq-no-sound",
        q: "Why is there no sound?",
        a: (
          <p>
            Sound effects are an <strong>optional WAV bank</strong>: only four sounds ship with the repo
            (card_deal, card_flip, card_place, koi_koi); missing files stay silent. Check both the{" "}
            <strong>Master Volume</strong> and <strong>Sound Effects</strong> meters in the pause menu, and that
            the <code>sfx/</code> folder sits beside the executable. See{" "}
            <Link to="extras">Gallery, Radio, Sound</Link>.
          </p>
        ),
      },
      {
        id: "faq-radio",
        q: "The radio doesn't play – why?",
        a: (
          <p>
            Playback needs a working audio device (<code>InitAudioDevice()</code>). On Linux and macOS the
            stream uses plain HTTP sockets – <strong>HTTPS-only streams won't play there</strong>; Windows
            handles HTTPS via WinHTTP. Playback also waits for 1.5 s of buffer. The console's{" "}
            <code>[radio]</code> lines (state changes, connection errors, stream format) tell you exactly where
            it fails.
          </p>
        ),
      },
      {
        id: "faq-browser",
        q: "Can I play in the browser?",
        a: (
          <p>
            Not currently – Densha de Koi is a desktop app (C++20 + Raylib), and the README documents no browser
            build. Its sibling project Xeno-koi has a WebAssembly version; DenshaDeKoi does not (yet).
          </p>
        ),
      },
      {
        id: "faq-tests",
        q: "How do I run the tests?",
        a: (
          <>
            <p>Headless, without Raylib or a window system:</p>
            <CodeBlock code={`cmake -S . -B build-headless -DBUILD_DESKTOP_APP=OFF -DBUILD_TESTING=ON\ncmake --build build-headless\nctest --test-dir build-headless --output-on-failure`} />
            <p>
              With Python 3 available, CTest also checks every card's black frame pixels. More in{" "}
              <Link to="entwicklung">Development</Link>.
            </p>
          </>
        ),
      },
      {
        id: "faq-contributing",
        q: "Can I contribute code?",
        a: (
          <p>
            Yes – outside the protected areas. Rules, card identities, the atlas, table layout and menu designs
            form a locked product contract that only the repository owner can approve changes to. Read{" "}
            <Link to="entwicklung/mitarbeit">Contributing & protected areas</Link> first.
          </p>
        ),
      },
    ],
  },
];

const ALL_IDS = GROUPS.flatMap((g) => g.items.map((i) => i.id));

export default function FaqPage() {
  const { route } = useRouter();
  const [open, setOpen] = useState<Set<string>>(new Set());

  // Direct links like #/faq/faq-radio open the matching answer.
  useEffect(() => {
    if (route.section && ALL_IDS.includes(route.section)) {
      setOpen((prev) => new Set(prev).add(route.section!));
    }
  }, [route.section]);

  const toggle = (id: string) => {
    setOpen((prev) => {
      const next = new Set(prev);
      if (next.has(id)) next.delete(id);
      else next.add(id);
      return next;
    });
  };

  return (
    <div>
      <PageHeader
        group="Reference"
        level="Beginner"
        title="FAQ & Help"
        lead="Quick answers to the questions that come up most. Every answer has a direct link – share it with a friend."
      />

      {GROUPS.map((group) => (
        <section key={group.title}>
          <H2 id={group.title === "Start & installation" ? "faq-start" : group.title === "Playing" ? "faq-playing" : "faq-tech"}>
            {group.title}
          </H2>
          <div className="space-y-3">
            {group.items.map((item) => (
              <AccordionItem
                key={item.id}
                id={item.id}
                question={item.q}
                open={open.has(item.id)}
                onToggle={() => toggle(item.id)}
              >
                {item.a}
              </AccordionItem>
            ))}
          </div>
        </section>
      ))}

      <p className="mt-10">
        Still stuck? The <Link to="glossar">Glossary</Link> decodes every term, and the header search (
        <kbd className="rounded-md border border-slate-200 bg-slate-50 px-1.5 py-0.5 font-mono text-[11px] font-semibold text-slate-600">Ctrl K</kbd>
        ) covers the whole wiki – including all 48 cards.
      </p>
    </div>
  );
}
