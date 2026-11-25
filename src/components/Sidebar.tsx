import { GROUPS, PAGES } from "@/lib/pages";
import { Link, useRouter } from "@/lib/router";

/** Line-map style navigation: every page is a station on the line. */
export function Sidebar({ onNavigate }: { onNavigate?: () => void }) {
  const { route } = useRouter();

  return (
    <nav className="px-5 py-6" aria-label="Wiki pages">
      {GROUPS.map((group) => {
        const pages = PAGES.filter((p) => p.group === group);
        return (
          <div key={group} className="mb-7 last:mb-0">
            <p className="mb-2.5 text-[11px] font-bold tracking-[0.18em] text-slate-400 uppercase">{group}</p>
            <div className="relative">
              <span className="absolute top-2 bottom-2 left-[8px] w-px bg-slate-200" aria-hidden />
              <ul className="space-y-0.5">
                {pages.map((p) => {
                  const active = route.page === p.id;
                  return (
                    <li key={p.id}>
                      <Link
                        to={p.id}
                        data-plain
                        onClick={onNavigate}
                        aria-current={active ? "page" : undefined}
                        className={`group relative flex items-center gap-3 rounded-lg py-2 pr-2 pl-0.5 text-sm transition ${
                          active ? "font-semibold text-navy-900" : "text-slate-500 hover:text-navy-800"
                        }`}
                      >
                        <span
                          className={`relative z-10 h-[15px] w-[15px] flex-none rounded-full border-2 transition ${
                            active
                              ? "border-koi-500 bg-koi-500 ring-4 ring-koi-100"
                              : "border-slate-300 bg-white group-hover:border-navy-400"
                          }`}
                          aria-hidden
                        />
                        <span className="truncate">{p.nav}</span>
                        {active && <span className="ml-auto h-1.5 w-1.5 flex-none rounded-full bg-koi-400" aria-hidden />}
                      </Link>
                    </li>
                  );
                })}
              </ul>
            </div>
          </div>
        );
      })}
      <p className="mt-8 border-t border-slate-100 pt-4 text-[11px] leading-5 text-slate-400">
        12 stations · 4 lines
        <br />
        Next stop: your first Koi-Koi.
      </p>
    </nav>
  );
}
