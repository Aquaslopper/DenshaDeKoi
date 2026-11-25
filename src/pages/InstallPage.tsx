import { Download, ExternalLink, Terminal } from "lucide-react";
import { Link } from "@/lib/router";
import { ACTIONS_URL, RELEASES_URL, REPO_URL } from "@/lib/cards";
import { Callout, CodeBlock, H2, PageHeader, Step, Steps, Table, Tabs } from "@/components/ui";

export default function InstallPage() {
  return (
    <div>
      <PageHeader
        group="First steps"
        level="Beginner"
        title="Installation"
        lead="From zero to a running game in four commands – or grab a prebuilt Windows download from the Releases page."
      />

      <Callout tone="pink" title="In short">
        <p>
          There is no classic installer. Either download a ready-made build from the{" "}
          <a href={RELEASES_URL} target="_blank" rel="noreferrer">Releases page</a> (currently the beta archives,
          e.g. <code>DENSHA.DE.KOI.v3.7z</code>), or build the game yourself with four commands:
        </p>
        <p>
          <code>git clone …</code> → <code>cmake -S . -B build</code> → <code>cmake --build build</code> → run{" "}
          <code>denshadekoi</code>.
        </p>
      </Callout>

      <H2 id="requirements">Requirements</H2>
      <Table head={["You need", "Details"]}>
        <tr>
          <td><strong>CMake</strong></td>
          <td>Version 3.24 or newer – the build system.</td>
        </tr>
        <tr>
          <td><strong>C++20 compiler</strong></td>
          <td>MSVC (Visual Studio), GCC or Clang.</td>
        </tr>
        <tr>
          <td><strong>Git</strong></td>
          <td>To clone the repository; CMake also uses it to fetch Raylib.</td>
        </tr>
        <tr>
          <td><strong>Raylib platform dependencies</strong></td>
          <td>
            The usual build dependencies for your system – see the{" "}
            <a href="https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux" target="_blank" rel="noreferrer">Raylib wiki</a>.
          </td>
        </tr>
        <tr>
          <td><strong>Raylib 5.5</strong></td>
          <td>Fetched automatically by CMake – you don't install it yourself.</td>
        </tr>
      </Table>

      <H2 id="systems">Pick your system</H2>
      <Tabs
        tabs={[
          {
            id: "windows",
            label: "Windows",
            content: (
              <>
                <p>The typical setup:</p>
                <ul>
                  <li><strong>Visual Studio 2022</strong> with the “Desktop development with C++” workload (MSVC), or MinGW-w64.</li>
                  <li><strong>CMake</strong> ≥ 3.24 and <strong>Git</strong> – both installers offer “add to PATH”.</li>
                  <li>Run the four commands below in a terminal (PowerShell or the VS developer prompt).</li>
                </ul>
                <p>
                  The CI builds with both MSVC and MinGW-w64, so either toolchain is known to work. Prefer not to
                  build? Use the <a href={RELEASES_URL} target="_blank" rel="noreferrer">Releases page</a> or the
                  CI artifacts described below.
                </p>
              </>
            ),
          },
          {
            id: "macos",
            label: "macOS",
            content: (
              <>
                <p>The typical setup:</p>
                <ul>
                  <li><strong>Xcode Command Line Tools</strong> (<code>xcode-select --install</code>) for the Clang compiler and Git.</li>
                  <li><strong>CMake</strong> ≥ 3.24, e.g. via Homebrew (<code>brew install cmake</code>).</li>
                  <li>Raylib's macOS dependencies are covered by the system frameworks; CMake fetches Raylib 5.5 itself.</li>
                </ul>
                <p>Then run the four commands below in Terminal.</p>
              </>
            ),
          },
          {
            id: "linux",
            label: "Linux",
            content: (
              <>
                <p>The typical setup:</p>
                <ul>
                  <li>A C++20 compiler (<code>build-essential</code> / GCC or Clang), <strong>CMake</strong> ≥ 3.24 and <strong>Git</strong>.</li>
                  <li>The Raylib platform build dependencies for your distribution (X11/OpenGL development packages and friends) – the <a href="https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux" target="_blank" rel="noreferrer">Raylib wiki</a> lists them per distro.</li>
                </ul>
                <p>Then run the four commands below in your terminal.</p>
              </>
            ),
          },
        ]}
      />

      <H2 id="four-steps">Build in four steps</H2>
      <Steps>
        <Step n={1} title="Clone the repository">
          <p>Download the source code from GitHub:</p>
          <CodeBlock label="Terminal" code={`git clone https://github.com/Aquaslopper/DenshaDeKoi.git\ncd DenshaDeKoi`} />
        </Step>
        <Step n={2} title="Configure the build">
          <p>
            CMake checks your compiler and fetches Raylib 5.5 automatically. This can take a few minutes on the
            first run.
          </p>
          <CodeBlock label="Terminal" code={`cmake -S . -B build`} />
        </Step>
        <Step n={3} title="Compile">
          <p>Build the game and its tools:</p>
          <CodeBlock label="Terminal" code={`cmake --build build`} />
        </Step>
        <Step n={4} title="Start the game" last>
          <p>On macOS and Linux:</p>
          <CodeBlock label="macOS / Linux" code={`cd build\n./denshadekoi`} />
          <p>
            On Windows with a multi-configuration generator (Visual Studio), run the executable from its
            configuration directory:
          </p>
          <CodeBlock label="Windows" code={`build\\Debug\\denshadekoi.exe`} />
        </Step>
      </Steps>

      <Callout tone="tip" title="Game files stay next to the .exe">
        <p>
          CMake copies all assets beside the executables – the card atlas, fonts, sounds and the{" "}
          <code>options/</code> menu textures. The game loads them from that directory regardless of your current
          working folder, so keep the files together when you move the build somewhere else.
        </p>
      </Callout>

      <H2 id="prebuilt">The ready-made Windows version</H2>
      <p>
        You don't have to build anything: the project publishes Windows downloads in two places.
      </p>
      <ul>
        <li>
          <strong>Releases page:</strong> packaged beta archives such as <code>DENSHA.DE.KOI.v3.7z</code> – extract
          them (e.g. with 7-Zip) and start the contained executable.{" "}
          <a href={RELEASES_URL} target="_blank" rel="noreferrer">Open Releases</a>.
        </li>
        <li>
          <strong>CI artifacts:</strong> every push and pull request is built on Windows with MSVC and MinGW-w64.
          Open the <a href={ACTIONS_URL} target="_blank" rel="noreferrer">Actions tab</a>, pick the latest
          successful “Build” run and download the artifact <code>denshadekoi-windows-msvc</code> (or{" "}
          <code>denshadekoi-windows-mingw</code>). It contains the staged game with all assets.
        </li>
      </ul>
      <Callout tone="info">
        <p>
          Downloading CI artifacts requires a GitHub login – that's a GitHub restriction, not a project one. The
          Releases page works without logging in.
        </p>
      </Callout>

      <H2 id="first-start">What happens on first start?</H2>
      <p>
        The game opens on its <strong>start menu</strong> – a painted station scene with five entries:{" "}
        <strong>Play</strong>, <strong>Options</strong>, <strong>Radio</strong>, <strong>Card Gallery</strong> and{" "}
        <strong>Exit</strong>. Four different background screens rotate through the menu.
      </p>
      <p>
        Choose <strong>Play</strong> to set up a match (number of rounds and route), then the opening dealer draw
        begins. Everything from there is covered in <Link to="steuerung">Controls</Link>.
      </p>

      <H2 id="tools">Extra tools for the curious</H2>
      <p>
        <strong>Atlas inspector.</strong> The build also produces <code>denshadekoi_inspector</code> (or{" "}
        <code>denshadekoi_inspector.exe</code>): a grid of all 48 atlas cells with hover identifiers. You can pass
        an alternative packed atlas path as its first argument.
      </p>
      <p>
        <strong>Headless tests.</strong> The rules, deck, table geometry and more can be verified without Raylib,
        a window system or network access:
      </p>
      <CodeBlock
        label="Terminal"
        code={`cmake -S . -B build-headless -DBUILD_DESKTOP_APP=OFF -DBUILD_TESTING=ON\ncmake --build build-headless\nctest --test-dir build-headless --output-on-failure`}
      />
      <p>
        When Python 3 is available, CTest additionally checks the black frame pixels on every card using only the
        Python standard library. More in <Link to="entwicklung">Development</Link>.
      </p>

      <div className="mt-10 flex flex-wrap items-center gap-3 rounded-2xl border border-slate-200 bg-slate-50/60 p-4 text-sm text-slate-500">
        <Download className="h-4 w-4 flex-none text-navy-400" />
        <span>
          Sources: <a href={REPO_URL} target="_blank" rel="noreferrer">README</a> and the{" "}
          <a href={ACTIONS_URL} target="_blank" rel="noreferrer">build workflow</a>.
        </span>
        <span className="ml-auto flex items-center gap-1.5">
          <Terminal className="h-4 w-4 text-navy-400" />
          <a href={REPO_URL} target="_blank" rel="noreferrer" className="inline-flex items-center gap-1">
            Repository <ExternalLink className="h-3 w-3" />
          </a>
        </span>
      </div>
    </div>
  );
}
