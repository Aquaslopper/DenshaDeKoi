import { useCallback, useEffect, useState, type ComponentType } from "react";
import { ArrowLeft, ArrowRight, Menu, Search, TrainFront, X } from "lucide-react";
import { PAGES, pageById } from "@/lib/pages";
import { Link, RouterProvider, useRouter } from "@/lib/router";
import { REPO_URL } from "@/lib/cards";
import { CardModalProvider } from "@/components/CardModal";
import { SearchDialog } from "@/components/Search";
import { Sidebar } from "@/components/Sidebar";
import { Toc } from "@/components/Toc";
import StartPage from "@/pages/StartPage";
import InstallPage from "@/pages/InstallPage";
import BasicsPage from "@/pages/BasicsPage";
import ControlsPage from "@/pages/ControlsPage";
import YakuPage from "@/pages/YakuPage";
import CardsPage from "@/pages/CardsPage";
import OptionsPage from "@/pages/OptionsPage";
import ExtrasPage from "@/pages/ExtrasPage";
import RulesPage from "@/pages/RulesPage";
import GlossaryPage from "@/pages/GlossaryPage";
import FaqPage from "@/pages/FaqPage";
import DevPage from "@/pages/DevPage";

const PAGE_COMPONENTS: Record<string, ComponentType> = {
  start: StartPage,
  installation: InstallPage,
  grundlagen: BasicsPage,
  steuerung: ControlsPage,
  yaku: YakuPage,
  karten: CardsPage,
  optionen: OptionsPage,
  extras: ExtrasPage,
  regelwerk: RulesPage,
  glossar: GlossaryPage,
  faq: FaqPage,
  entwicklung: DevPage,
};

function GithubMark({ className }: { className?: string }) {
  return (
    <svg viewBox="0 0 16 16" className={className} fill="currentColor" aria-hidden>
      <path d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 0 0016 8c0-4.42-3.58-8-8-8z" />
    </svg>
  );
}

function Header({ onMenu, onSearch }: { onMenu: () => void; onSearch: () => void }) {
  return (
    <header className="sticky top-0 z-40 border-b border-slate-200/80 bg-white/90 backdrop-blur-md">
      <div className="rail-stripe h-1" />
      <div className="mx-auto flex h-14 max-w-[90rem] items-center gap-3 px-4 sm:px-6">
        <button onClick={onMenu} className="-ml-1 flex h-10 w-10 items-center justify-center rounded-lg text-slate-500 hover:bg-slate-100 lg:hidden" aria-label="Open menu">
          <Menu className="h-5 w-5" />
        </button>

        <Link to="start" data-plain className="flex items-center gap-2.5">
          <span className="flex h-9 w-9 items-center justify-center rounded-xl bg-navy-800 text-white shadow-sm">
            <TrainFront className="h-5 w-5" />
          </span>
          <span className="leading-none">
            <span className="block font-display text-[17px] font-black tracking-tight text-navy-900">
              Densha de <span className="text-koi-500">Koi</span>
            </span>
            <span className="mt-0.5 block text-[10px] font-semibold uppercase tracking-[0.2em] text-slate-400">Wiki · 電車で恋</span>
          </span>
        </Link>

        <div className="flex-1" />

        <button
          onClick={onSearch}
          className="group flex h-10 w-10 items-center gap-2.5 rounded-xl border border-slate-200 bg-slate-50/70 text-sm text-slate-400 transition hover:border-navy-200 hover:bg-white sm:w-72 sm:px-3.5"
          aria-label="Open search"
        >
          <Search className="mx-auto h-4 w-4 flex-none sm:mx-0" />
          <span className="hidden flex-1 text-left sm:block">Search the wiki …</span>
          <kbd className="hidden rounded-md border border-slate-200 bg-white px-1.5 py-0.5 font-mono text-[11px] text-slate-400 sm:block">Ctrl K</kbd>
        </button>

        <a
          href={REPO_URL}
          target="_blank"
          rel="noreferrer"
          className="hidden h-10 items-center gap-2 rounded-xl bg-navy-900 px-4 text-sm font-semibold text-white transition hover:bg-navy-700 md:flex"
        >
          <GithubMark className="h-4 w-4" />
          GitHub
        </a>
      </div>
    </header>
  );
}

function PrevNext() {
  const { route } = useRouter();
  const i = PAGES.findIndex((p) => p.id === route.page);
  const prev = PAGES[i - 1];
  const next = PAGES[i + 1];
  return (
    <div className="mt-16 grid gap-3 border-t border-slate-100 pt-8 sm:grid-cols-2">
      {prev ? (
        <Link
          to={prev.id}
          data-plain
          className="group flex items-center gap-3 rounded-2xl border border-slate-200 p-4 transition hover:border-navy-200 hover:bg-navy-50/50"
        >
          <ArrowLeft className="h-4 w-4 flex-none text-slate-400 transition group-hover:-translate-x-0.5 group-hover:text-koi-500" />
          <span className="min-w-0">
            <span className="block text-xs text-slate-400">Previous</span>
            <span className="block truncate font-display font-bold text-navy-900">{prev.nav}</span>
          </span>
        </Link>
      ) : (
        <span />
      )}
      {next && (
        <Link
          to={next.id}
          data-plain
          className="group flex items-center justify-end gap-3 rounded-2xl border border-slate-200 p-4 text-right transition hover:border-navy-200 hover:bg-navy-50/50"
        >
          <span className="min-w-0">
            <span className="block text-xs text-slate-400">Next</span>
            <span className="block truncate font-display font-bold text-navy-900">{next.nav}</span>
          </span>
          <ArrowRight className="h-4 w-4 flex-none text-slate-400 transition group-hover:translate-x-0.5 group-hover:text-koi-500" />
        </Link>
      )}
    </div>
  );
}

function Footer() {
  return (
    <footer className="mt-12 border-t border-slate-100 pt-8 pb-4 text-sm leading-6 text-slate-400">
      <p className="mb-2">
        <strong className="font-semibold text-slate-500">Densha de Koi Wiki</strong> – built from the public{" "}
        <a data-plain className="text-navy-600 underline decoration-navy-200 underline-offset-2 hover:text-koi-600" href={REPO_URL} target="_blank" rel="noreferrer">
          GitHub repository
        </a>{" "}
        (README, source code and card gallery).
      </p>
      <p>
        Game graphics and card images come from the repository; all rights remain with the project author. This
        wiki does not replace the README – when in doubt, the current state of the repository wins.
      </p>
    </footer>
  );
}

function Shell() {
  const { route } = useRouter();
  const [drawer, setDrawer] = useState(false);
  const [searchOpen, setSearchOpen] = useState(false);
  const Page = PAGE_COMPONENTS[route.page] ?? StartPage;

  // Scroll behaviour & title on page change
  useEffect(() => {
    const meta = pageById(route.page);
    document.title = meta && route.page !== "start" ? `${meta.nav} – Densha de Koi Wiki` : "Densha de Koi Wiki – Koi-Koi explained simply";
    if (route.section && !route.section.startsWith("card-")) {
      const t = window.setTimeout(() => {
        document.getElementById(route.section!)?.scrollIntoView({ behavior: "smooth", block: "start" });
      }, 60);
      return () => window.clearTimeout(t);
    }
    window.scrollTo({ top: 0, behavior: "instant" as ScrollBehavior });
  }, [route.page, route.section]);

  // Global shortcuts: Ctrl/Cmd+K or "/" opens the search
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      const tag = (e.target as HTMLElement | null)?.tagName;
      const typing = tag === "INPUT" || tag === "TEXTAREA";
      if ((e.metaKey || e.ctrlKey) && e.key.toLowerCase() === "k") {
        e.preventDefault();
        setSearchOpen(true);
      } else if (e.key === "/" && !typing) {
        e.preventDefault();
        setSearchOpen(true);
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, []);

  useEffect(() => {
    document.body.style.overflow = drawer ? "hidden" : "";
  }, [drawer]);

  const closeDrawer = useCallback(() => setDrawer(false), []);

  return (
    <div className="min-h-screen bg-white">
      <Header onMenu={() => setDrawer(true)} onSearch={() => setSearchOpen(true)} />

      {/* Mobile navigation */}
      {drawer && (
        <div className="fixed inset-0 z-50 lg:hidden">
          <button aria-label="Close menu" className="absolute inset-0 bg-navy-900/40 backdrop-blur-sm" onClick={closeDrawer} />
          <div className="pop-in absolute inset-y-0 left-0 flex w-[19rem] max-w-[86vw] flex-col bg-white shadow-2xl">
            <div className="flex h-14 items-center justify-between border-b border-slate-100 px-5">
              <span className="font-display font-black text-navy-900">Navigation</span>
              <button onClick={closeDrawer} className="flex h-9 w-9 items-center justify-center rounded-lg text-slate-500 hover:bg-slate-100" aria-label="Close">
                <X className="h-5 w-5" />
              </button>
            </div>
            <div className="scrollbar-thin flex-1 overflow-y-auto">
              <Sidebar onNavigate={closeDrawer} />
            </div>
          </div>
        </div>
      )}

      <div className="mx-auto flex max-w-[90rem]">
        <aside className="scrollbar-thin sticky top-[3.75rem] hidden h-[calc(100vh-3.75rem)] w-72 flex-none overflow-y-auto border-r border-slate-100 lg:block">
          <Sidebar />
        </aside>

        <main className="min-w-0 flex-1 px-5 py-10 sm:px-8 lg:px-12 lg:py-12">
          <div className="mx-auto max-w-3xl">
            <div id="wiki-content" key={route.page} className="wiki fade-up">
              <Page />
            </div>
            <PrevNext />
            <Footer />
          </div>
        </main>

        <aside className="sticky top-[3.75rem] hidden h-[calc(100vh-3.75rem)] w-60 flex-none overflow-y-auto xl:block">
          <Toc />
        </aside>
      </div>

      {/* Remounted on every open: always an empty field with autofocus */}
      {searchOpen && <SearchDialog onClose={() => setSearchOpen(false)} />}
    </div>
  );
}

export default function App() {
  return (
    <RouterProvider>
      <CardModalProvider>
        <Shell />
      </CardModalProvider>
    </RouterProvider>
  );
}
