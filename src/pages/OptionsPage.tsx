import { Eye } from "lucide-react";
import pause from "@/assets/pause.jpg";
import { Link } from "@/lib/router";
import { Callout, H2, Kbd, PageHeader, Panel, Table } from "@/components/ui";

const OPTIONS = [
  {
    n: 1,
    name: "Master Volume",
    text: "The final mix – everything you hear, including the radio. Drag the meter to set it.",
  },
  {
    n: 2,
    name: "Sound Effects",
    text: "Controls only the optional WAV sound bank (card deals, flips, Koi-Koi …). The radio has its own volume.",
  },
  {
    n: 3,
    name: "Card Hints",
    text: "Outlines matching field cards while you hover or drag a playable hand card, and the legal targets when a stock card needs a choice. Switching it off also hides the drag tip – legal moves and right-click inspection stay the same.",
  },
  {
    n: 4,
    name: "Display Resolution",
    text: "Cycles through presets that fit your current monitor. The readout shows the active size, including manual window resizes.",
  },
  {
    n: 5,
    name: "Screen Mode",
    text: "Windowed, Borderless or Fullscreen. Borderless uses the desktop size and disables the resolution arrows; exclusive fullscreen picks the nearest mode your display supports.",
  },
  {
    n: 6,
    name: "Anti-Aliasing",
    text: "The table's supersampling density: 2×, 3× or 4×. The 3D world is downsampled before the UI is drawn – camera, card positions and hit areas stay fixed.",
  },
];

export default function OptionsPage() {
  return (
    <div>
      <PageHeader
        group="Menus & extras"
        level="Beginner"
        title="Menus & Options"
        lead="The start menu, match settings and the six pause-menu options – what each one does and where your settings live."
      />

      <H2 id="start-menu">The start menu</H2>
      <p>
        The game opens on a painted station scene with five entries. Four different 16:9 background screens
        rotate through the menu, so it never looks quite the same twice.
      </p>
      <Table head={["Entry", "What it does"]}>
        <tr>
          <td><strong>Play</strong></td>
          <td>Opens the match settings, then starts a match against the CPU.</td>
        </tr>
        <tr>
          <td><strong>Options</strong></td>
          <td>The same six settings as the pause menu – volume, hints, display and anti-aliasing.</td>
        </tr>
        <tr>
          <td><strong>Radio</strong></td>
          <td>The J-Pop internet radio panel with six Asia DREAM Radio stations.</td>
        </tr>
        <tr>
          <td><strong>Card Gallery</strong></td>
          <td>Browse all 48 cards in original resolution, with a 3D pop-out viewer.</td>
        </tr>
        <tr>
          <td><strong>Exit</strong></td>
          <td>Leaves the game. Settings are saved on normal exit.</td>
        </tr>
      </Table>

      <H2 id="match-settings">Match settings</H2>
      <p>After pressing Play you configure the match:</p>
      <ul>
        <li>
          <strong>Number of Rounds:</strong> 1, 3, 6 or 12. The standard match is <strong>3 rounds</strong>.
        </li>
        <li>
          <strong>Route:</strong> currently <strong>The City</strong> – the station-and-table scene. The selector
          is built for more routes, but this is the one shipped today.
        </li>
        <li>
          <strong>Start</strong> begins the dealer draw; <strong>Back</strong> returns to the start menu. Arrow
          keys cycle the round and route choices.
        </li>
      </ul>

      <H2 id="pause-menu">The pause menu</H2>
      <p>
        Press <Kbd>Esc</Kbd> or <Kbd>P</Kbd> (or click <strong>PAUSE</strong>) during a match. The game freezes
        beside the Tokyo train and six numbered option strips appear:
      </p>
      <figure className="my-6">
        <img
          src={pause}
          alt="The in-game pause menu: six numbered option strips beside the Tokyo train scene"
          className="w-full rounded-2xl border border-slate-200 shadow-sm"
        />
        <figcaption className="mt-2.5 text-center text-[13px] text-slate-400">
          The pause menu with its six option strips and the illustrated Resume button.
        </figcaption>
      </figure>

      <div className="grid gap-3 sm:grid-cols-2">
        {OPTIONS.map((o) => (
          <Panel key={o.n}>
            <p className="flex items-center gap-2.5 font-display text-[15px] font-bold text-navy-900">
              <span className="flex h-7 w-7 flex-none items-center justify-center rounded-full bg-koi-500 text-xs font-black text-white">
                {o.n}
              </span>
              {o.name}
            </p>
            <p className="mt-2 text-sm leading-6 text-slate-500">{o.text}</p>
          </Panel>
        ))}
      </div>

      <H2 id="operating">Operating the pause menu</H2>
      <ul>
        <li><strong>Drag</strong> the level meters (Master Volume, Sound Effects) with the mouse.</li>
        <li><strong>Click</strong> a choice (hints on/off, resolution, screen mode, anti-aliasing).</li>
        <li>
          Or use the keyboard: <Kbd>↑</Kbd> / <Kbd>↓</Kbd> selects a row, <Kbd>←</Kbd> / <Kbd>→</Kbd> adjusts it.
        </li>
        <li>
          Click the illustrated <strong>Resume</strong> button – or select it and press <Kbd>Enter</Kbd> – to
          return to the <em>same</em> match. <Kbd>Esc</Kbd> resumes too.
        </li>
        <li>All changes take effect immediately.</li>
      </ul>

      <Callout tone="tip" title="Beginner tip">
        <p>
          Keep <strong>Card Hints</strong> on <Eye className="inline h-4 w-4 text-emerald-600" /> while you
          learn. Matching field cards get outlined while you drag, so you always see your legal targets – it
          changes nothing about the rules, only what you see.
        </p>
      </Callout>

      <H2 id="save-location">Where settings are saved</H2>
      <p>
        On normal exit, everything is written to <code>options.txt</code> inside a <code>DenshaDeKoi</code>{" "}
        folder:
      </p>
      <Table head={["System", "Location"]}>
        <tr>
          <td>Windows</td>
          <td><code>%APPDATA%\DenshaDeKoi\options.txt</code></td>
        </tr>
        <tr>
          <td>macOS</td>
          <td><code>~/Library/Application Support/DenshaDeKoi/options.txt</code></td>
        </tr>
        <tr>
          <td>Linux</td>
          <td><code>$XDG_CONFIG_HOME/DenshaDeKoi/options.txt</code> (or <code>~/.config/DenshaDeKoi/options.txt</code>)</td>
        </tr>
      </Table>
      <p>
        <strong>Reset everything:</strong> delete the file – the game restores its defaults on the next start.
      </p>

      <p>
        The gallery and radio behind entries 3 and 4 have their own page:{" "}
        <Link to="extras">Gallery, Radio, Sound</Link>.
      </p>
    </div>
  );
}
