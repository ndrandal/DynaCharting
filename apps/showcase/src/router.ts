/* apps/showcase/src/router.ts
 *
 * A tiny hash router for the four-act narrative (DESIGN §1): `/` hero,
 * `/gallery`, `/view/:id`, `/frontier`, `/report`. Hash-based so the showcase
 * stays a static deploy (no server rewrites). No dependency — the routes are
 * few and fixed.
 */

import { useEffect, useState } from 'react';

export type Route =
  | { name: 'home' }
  | { name: 'gallery' }
  | { name: 'view'; id: string }
  | { name: 'frontier' }
  | { name: 'report' };

export function routeToHash(r: Route): string {
  switch (r.name) {
    case 'home':
      return '#/';
    case 'gallery':
      return '#/gallery';
    case 'view':
      return `#/view/${r.id}`;
    case 'frontier':
      return '#/frontier';
    case 'report':
      return '#/report';
  }
}

export function parseHash(hash: string): Route {
  const h = hash.replace(/^#/, '');
  const parts = h.split('/').filter(Boolean);
  if (parts.length === 0) return { name: 'home' };
  switch (parts[0]) {
    case 'gallery':
      return { name: 'gallery' };
    case 'view':
      return parts[1] ? { name: 'view', id: decodeURIComponent(parts[1]) } : { name: 'gallery' };
    case 'frontier':
      return { name: 'frontier' };
    case 'report':
      return { name: 'report' };
    default:
      return { name: 'home' };
  }
}

export function useRouter(): { route: Route; navigate: (r: Route) => void } {
  const [route, setRoute] = useState<Route>(() => parseHash(window.location.hash));

  useEffect(() => {
    const onHash = () => setRoute(parseHash(window.location.hash));
    window.addEventListener('hashchange', onHash);
    return () => window.removeEventListener('hashchange', onHash);
  }, []);

  const navigate = (r: Route) => {
    const next = routeToHash(r);
    if (window.location.hash !== next) window.location.hash = next;
    else setRoute(r); // same hash → force-set (e.g. re-select current view)
  };

  return { route, navigate };
}
