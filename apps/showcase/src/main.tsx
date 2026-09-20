import ReactDOM from 'react-dom/client';
import App from './App';
import { AppBoundary } from './components/ErrorBoundary';
import './App.css';

// NOTE: We deliberately do NOT wrap <App /> in <React.StrictMode>. StrictMode
// double-invokes effects and the canvas callback ref (mount → unmount → mount)
// in dev, which would create/destroy two EngineHost instances bound to the same
// canvas. The WASM renderer uses ASYNCIFY (one async GPU op in flight at a
// time), so a redundant second host racing the first aborts the module
// ("cannot have multiple async operations in flight at once"). A single, stable
// engine lifecycle is the correct model for a WASM/WebGPU render shell.
// ENC-1313: the root boundary. It cannot keep anything running — by the time it
// fires, App's tree is already gone — but it replaces the BLANK PAGE that a
// throw above every boundary used to produce with a named, copyable diagnostic.
// The worst property of the crash this ticket fixes was not that it crashed: it
// was that "the app died" and "the app has not loaded yet" were pixel-identical,
// so the failure read as a slow load for as long as nobody deep-linked twice.
ReactDOM.createRoot(document.getElementById('root')!).render(
  <AppBoundary>
    <App />
  </AppBoundary>,
);
