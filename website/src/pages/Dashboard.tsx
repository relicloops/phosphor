import { css } from '@relicloops/cathode';

import { BackToTop } from '../components/BackToTop';
import { Footer } from '../components/Footer';
import { Header } from '../components/Header';

export const Dashboard = () => {

  /* fonts */
  css( './css/fonts/share-tech-mono.css' );
  /* theme */
  css( './css/theme.css' );
  /* component styles */
  css( './css/pages/dashboard.css' );

  return (
    <div>
      <Header />
      <main class="dash">
        <h1 class="dash__title">Dashboard</h1>
        <p class="dash__intro">
          The phosphor serve dashboard is an ncurses TUI that monitors child
          processes in real time. It captures stdout/stderr from neonsignal,
          neonsignal_redirect, and a file watcher into scrollable, color-coded
          panels with a poll()-based event loop.
        </p>

        <div class="dash__cards">
          <a href="/dashboard-manual.html" class="dash__card">
            <h2 class="dash__card-title">Manual</h2>
            <p class="dash__card-desc">
              Complete user guide. Keybindings, commands, modes, popups,
              search, selection, save/export, fuzzy log finder, JSON viewer,
              and viewport behavior -- everything you need to operate the
              dashboard.
            </p>
            <span class="dash__card-arrow">&rarr;</span>
          </a>

          <a href="/dashboard-implementation.html" class="dash__card">
            <h2 class="dash__card-title">Implementation</h2>
            <p class="dash__card-desc">
              Technical architecture. Event loop internals, file layout,
              collect/dispatch/render cycle, signal handling, process
              lifecycle, concurrency model, and the UI mode state machine.
            </p>
            <span class="dash__card-arrow">&rarr;</span>
          </a>
        </div>
      </main>
      <BackToTop />
      <Footer />
    </div>
  );
};
