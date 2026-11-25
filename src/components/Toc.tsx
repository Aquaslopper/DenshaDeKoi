import { useEffect, useState } from "react";
import { useRouter } from "@/lib/router";

interface TocEntry {
  id: string;
  text: string;
}

/** Right-hand table of contents, built from h2[data-toc] with scroll marking. */
export function Toc() {
  const { route, navigate } = useRouter();
  const [entries, setEntries] = useState<TocEntry[]>([]);
  const [activeId, setActiveId] = useState<string>("");

  useEffect(() => {
    const collect = () => {
      const nodes = Array.from(document.querySelectorAll<HTMLElement>("#wiki-content h2[data-toc]"));
      setEntries(nodes.map((n) => ({ id: n.id, text: n.textContent ?? "" })));
      setActiveId(nodes[0]?.id ?? "");
    };
    collect();
    // Content (fonts/images) can settle a tick later – collect once more.
    const t = window.setTimeout(collect, 120);

    const observer = new IntersectionObserver(
      (observed) => {
        for (const o of observed) {
          if (o.isIntersecting) {
            setActiveId((o.target as HTMLElement).id);
            break;
          }
        }
      },
      { rootMargin: "-90px 0px -65% 0px", threshold: 0 },
    );
    document.querySelectorAll("#wiki-content h2[data-toc]").forEach((n) => observer.observe(n));
    return () => {
      window.clearTimeout(t);
      observer.disconnect();
    };
  }, [route.page]);

  if (entries.length === 0) return null;

  return (
    <nav className="px-5 py-8" aria-label="On this page">
      <p className="mb-3 text-[11px] font-bold tracking-[0.18em] text-slate-400 uppercase">On this page</p>
      <ul className="space-y-0.5 border-l border-slate-200">
        {entries.map((e) => (
          <li key={e.id}>
            <button
              onClick={() => {
                document.getElementById(e.id)?.scrollIntoView({ behavior: "smooth", block: "start" });
                navigate(`${route.page}/${e.id}`);
              }}
              className={`-ml-px block w-full border-l-2 py-1.5 pr-2 pl-3 text-left text-[13px] leading-5 transition ${
                activeId === e.id
                  ? "border-koi-500 font-semibold text-koi-600"
                  : "border-transparent text-slate-500 hover:border-slate-300 hover:text-navy-800"
              }`}
            >
              {e.text}
            </button>
          </li>
        ))}
      </ul>
    </nav>
  );
}
