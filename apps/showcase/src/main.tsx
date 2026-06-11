import ReactDOM from 'react-dom/client';
import App from './App';
import './App.css';

// NOTE: We deliberately do NOT wrap <App /> in <React.StrictMode>. StrictMode
// double-invokes effects and the canvas callback ref (mount → unmount → mount)
// in dev, which would create/destroy two EngineHost instances bound to the same
// canvas. The WASM renderer uses ASYNCIFY (one async GPU op in flight at a
// time), so a redundant second host racing the first aborts the module
// ("cannot have multiple async operations in flight at once"). A single, stable
// engine lifecycle is the correct model for a WASM/WebGPU render shell.
ReactDOM.createRoot(document.getElementById('root')!).render(<App />);
