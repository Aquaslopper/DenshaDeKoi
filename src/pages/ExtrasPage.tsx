import { Music, Radio, Volume2 } from "lucide-react";
import { Link } from "@/lib/router";
import { Callout, H2, Kbd, PageHeader, Pill, Table } from "@/components/ui";

const STATIONS = [
  { name: "J-Pop Powerplay", desc: "Asia DREAM Radio's main J-Pop channel" },
  { name: "J-Pop Powerplay Kawaii", desc: "Cute J-Pop & idols" },
  { name: "J-Pop Sakura", desc: "Nostalgic 80s–00s" },
  { name: "Japan Hits", desc: "Tokyo top charts" },
  { name: "J-Rock Powerplay", desc: "J-Rock & anime" },
  { name: "Jazz Sakura", desc: "Japanese smooth jazz" },
];

const SFX = [
  { file: "card_deal.wav", slot: "A card leaving the stock", shipped: true },
  { file: "card_flip.wav", slot: "Revealing a card", shipped: true },
  { file: "card_place.wav", slot: "A card landing on the table", shipped: true },
  { file: "card_match.wav", slot: "A successful month match", shipped: false },
  { file: "card_capture.wav", slot: "Cards moving to a capture pile", shipped: false },
  { file: "koi_koi.wav", slot: "Choosing Koi-Koi", shipped: true },
  { file: "shobu.wav", slot: "Choosing Shobu", shipped: false },
  { file: "round_win.wav", slot: "Player wins a round", shipped: false },
  { file: "round_lose.wav", slot: "CPU wins a round", shipped: false },
];

export default function ExtrasPage() {
  return (
    <div>
      <PageHeader
        group="Menus & extras"
        level="Beginner"
        title="Gallery, Radio, Sound"
        lead="The 3D card viewer, six J-Pop radio stations and the optional sound-effect bank – the game's cozy corners."
      />

      <H2 id="gallery">The card gallery</H2>
      <p>
        Open <strong>Card Gallery</strong> from the start menu to browse all 48 cards in original resolution
        (602 × 928). The case is organized by month; the big card in the middle is your selection.
      </p>
      <Table head={["Input", "What it does"]}>
        <tr>
          <td><Kbd>↑</Kbd> / <Kbd>↓</Kbd></td>
          <td>Change the month.</td>
        </tr>
        <tr>
          <td><Kbd>←</Kbd> / <Kbd>→</Kbd></td>
          <td>Change the card within the month.</td>
        </tr>
        <tr>
          <td>Hover / click</td>
          <td>Select a month or a thumbnail directly.</td>
        </tr>
        <tr>
          <td>Click the big card / <Kbd>Enter</Kbd></td>
          <td>Pop it out as a real 3D card.</td>
        </tr>
        <tr>
          <td><Kbd>Esc</Kbd> / BACK</td>
          <td>Close the viewer (first press) or the gallery (second press).</td>
        </tr>
      </Table>

      <H2 id="viewer">The 3D card viewer</H2>
      <p>
        The pop-out animation lands exactly on the card's place in the case – then it's yours to inspect:
      </p>
      <ul>
        <li>
          <strong>Hold the left mouse button and move</strong> to turn the card. Dragging right brings the left
          edge toward you; dragging down brings the top edge toward you. Pitch is limited to <strong>±70°</strong>.
        </li>
        <li>
          <strong>The wheel zooms</strong> from <strong>0.6× to 3.0×</strong>, and the ZOOM readout follows.
        </li>
        <li>
          Turn it around to see the <strong>solid black back</strong> the table uses, with a cream edge around
          the card.
        </li>
        <li>
          Release mid-drag and a short <strong>inertia</strong> keeps the card turning – satisfying every time.
        </li>
        <li>
          <Kbd>Esc</Kbd>, a right click or <strong>BACK</strong> closes only the viewer; a second <Kbd>Esc</Kbd>{" "}
          leaves the gallery as before.
        </li>
      </ul>
      <Callout tone="info">
        <p>
          The card is rendered once into a render texture, supersampled by the <strong>Anti-Aliasing</strong>{" "}
          setting from the pause menu – so a higher AA setting also makes the viewer crisper.
        </p>
      </Callout>

      <H2 id="radio">Internet radio</H2>
      <p>
        The separate <strong>RADIO</strong> button pauses the game and opens the J-Pop radio panel{" "}
        <Radio className="inline h-4 w-4 text-koi-500" />. Six Asia DREAM Radio stations are built in:
      </p>
      <Table head={["Station", "Flavour"]}>
        {STATIONS.map((s) => (
          <tr key={s.name}>
            <td><strong>{s.name}</strong></td>
            <td>{s.desc}</td>
          </tr>
        ))}
      </Table>
      <ul>
        <li>A <strong>volume slider</strong> sets the radio level; <Kbd>M</Kbd> mutes and unmutes.</li>
        <li>The game stays paused while the panel is open – the radio is its own little lounge.</li>
        <li>
          Playback runs through Raylib's own <code>AudioStream</code>, so <code>InitAudioDevice()</code> must
          succeed. The console prints <code>[radio]</code> lines (state changes, connection errors, stream
          format) that show exactly where a failure happens.
        </li>
      </ul>

      <H2 id="radio-tech">How the stream works (for the curious)</H2>
      <p>
        The stream stack is deliberately small: a background thread resolves the station's <code>.pls</code>,
        reads the SHOUTcast MP3 stream over <strong>WinHTTP</strong> on Windows (including HTTPS) or a plain{" "}
        <strong>HTTP/1.0 socket</strong> on Linux/macOS (HTTP only), decodes it with the vendored{" "}
        <code>dr_mp3.h</code>, and pushes PCM into a mutex-protected queue.
      </p>
      <p>
        Playback starts once <strong>1.5 seconds</strong> are buffered. If the connection drops, the engine
        reconnects and keeps playing from what is still buffered.
      </p>
      <Callout tone="warn" title="Platform note">
        <p>
          Because Linux and macOS use plain HTTP sockets, HTTPS-only streams won't play there – Windows handles
          HTTPS via WinHTTP. If a station stays silent, check the <code>[radio]</code> console output first.
        </p>
      </Callout>

      <H2 id="sfx">Sound effects</H2>
      <p>
        The game supports an optional WAV bank in the <code>sfx/</code> folder beside the executable{" "}
        <Volume2 className="inline h-4 w-4 text-navy-400" />. All files are optional – a missing file simply
        stays silent. Names are lowercase and must match exactly:
      </p>
      <Table head={["File", "Sound slot", "In the repo"]}>
        {SFX.map((s) => (
          <tr key={s.file}>
            <td><code>{s.file}</code></td>
            <td>{s.slot}</td>
            <td>{s.shipped ? <Pill tone="emerald">included</Pill> : <Pill tone="slate">add your own</Pill>}</td>
          </tr>
        ))}
      </Table>
      <p>
        Four sounds ship with the repository (<code>card_deal</code>, <code>card_flip</code>,{" "}
        <code>card_place</code>, <code>koi_koi</code>); the rest are slots you can fill with your own WAVs. The
        build copies the directory beside the executable, and the <strong>Sound Effects</strong> volume stays
        separate from the radio volume.
      </p>

      <div className="mt-8 flex items-center gap-3 rounded-2xl border border-slate-200 bg-slate-50/60 p-4 text-sm text-slate-500">
        <Music className="h-4 w-4 flex-none text-navy-400" />
        <span>
          Volume controls for all of this live in the pause menu – see{" "}
          <Link to="optionen">Menus & Options</Link>.
        </span>
      </div>
    </div>
  );
}
