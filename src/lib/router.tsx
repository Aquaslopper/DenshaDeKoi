import {
  createContext,
  useContext,
  useEffect,
  useState,
  type AnchorHTMLAttributes,
  type ReactNode,
} from "react";

export interface Route {
  page: string;
  section: string | null;
}

function parseHash(): Route {
  const raw = window.location.hash.replace(/^#\/?/, "");
  const slash = raw.indexOf("/");
  const page = (slash === -1 ? raw : raw.slice(0, slash)) || "start";
  const section = slash === -1 ? null : raw.slice(slash + 1) || null;
  return { page, section };
}

interface RouterValue {
  route: Route;
  navigate: (to: string) => void;
}

const RouterContext = createContext<RouterValue>({
  route: { page: "start", section: null },
  navigate: () => {},
});

/** Minimal hash router: `#/page` and `#/page/section`. Works from a single file. */
export function RouterProvider({ children }: { children: ReactNode }) {
  const [route, setRoute] = useState<Route>(parseHash);

  useEffect(() => {
    const onHash = () => setRoute(parseHash());
    window.addEventListener("hashchange", onHash);
    return () => window.removeEventListener("hashchange", onHash);
  }, []);

  const navigate = (to: string) => {
    const target = `#/${to.replace(/^#?\/?/, "")}`;
    if (window.location.hash === target) {
      // Same hash: no hashchange event will fire – apply directly.
      setRoute(parseHash());
    } else {
      window.location.hash = target;
    }
  };

  return <RouterContext.Provider value={{ route, navigate }}>{children}</RouterContext.Provider>;
}

export function useRouter(): RouterValue {
  return useContext(RouterContext);
}

interface LinkProps extends AnchorHTMLAttributes<HTMLAnchorElement> {
  to: string;
}

export function Link({ to, children, ...rest }: LinkProps) {
  return (
    <a href={`#/${to.replace(/^#?\/?/, "")}`} {...rest}>
      {children}
    </a>
  );
}
