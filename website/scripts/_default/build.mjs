#!/usr/bin/env node
import * as esbuild                                                    from 'esbuild';
import { cathodeHMRPlugin }                                            from '@relicloops/cathode/esbuild-plugin';
import { cpSync, mkdirSync, existsSync, readFileSync, readdirSync,
         writeFileSync, unlinkSync }                                   from 'node:fs';
import { resolve, dirname, basename }                                  from 'node:path';
import { fileURLToPath }                                               from 'node:url';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '../..');

// Compute default public dir from SNI + TLD env vars (matches phosphor build logic).
// Falls back to public/_default only when neither SNI nor the explicit env var is set.
function resolveDefaultPublicDir() {
  if (process.env.PHOSPHOR_WEBSITE_PUBLIC_DIR)
    return process.env.PHOSPHOR_WEBSITE_PUBLIC_DIR;
  const sni = process.env.SNI;
  const tld = process.env.TLD ?? '';
  if (sni)
    return resolve(ROOT, 'public', `${sni}${tld}`);
  return resolve(ROOT, 'public/_default');
}

// Defaults
let sourceDir = process.env.PHOSPHOR_WEBSITE_SOURCE_DIR ?? resolve(ROOT, 'src');
let buildDir  = process.env.PHOSPHOR_WEBSITE_BUILD_DIR  ?? resolve(ROOT, 'build/src');
let publicDir = resolveDefaultPublicDir();
let watchMode = false;

const args = process.argv.slice(2);
for (let i = 0; i < args.length; i++) {
  switch (args[i]) {
    case '--build':          buildDir  = args[++i]; break;
    case '--public':
    case '--deploy':         publicDir = resolve(args[++i]); break;
    case '--watch':          watchMode = true;      break;
    case '--help': case '-h':
      console.log('Usage: node build.mjs [--build <dir>] [--public <dir>] [--watch]');
      process.exit(0);
  }
}

// Static assets are deployed to publicDir once before the build context starts.
// They are NEVER touched again on subsequent rebuilds -- this prevents NeonSignal
// HMR from flooding the browser with CSS/HTML/media change events that the
// browser then tries (and fails) to import() as ES modules.
function deployStaticOnce() {
  mkdirSync(buildDir,  { recursive: true });
  mkdirSync(publicDir, { recursive: true });
  const staticDir = resolve(sourceDir, 'static');
  if (existsSync(staticDir)) {
    cpSync(staticDir, publicDir, { recursive: true });
  }
}

deployStaticOnce();

// Collect all .tsx files under a directory tree.
// Used in watch mode to turn every component and page into its own named
// entry point, giving esbuild -- and therefore NeonSignal HMR -- the finest
// possible granularity: editing Header.tsx rewrites only Header.js.
function collectTsxFiles(dir) {
  const results = [];
  let entries;
  try { entries = readdirSync(dir, { withFileTypes: true }); }
  catch { return results; /* directory doesn't exist */ }
  for (const entry of entries) {
    const full = resolve(dir, entry.name);
    if (entry.isDirectory()) {
      results.push(...collectTsxFiles(full));
    } else if (entry.name.endsWith('.tsx')) {
      results.push(full);
    }
  }
  return results;
}

// In watch mode every .tsx file in pages/ and components/ becomes its own
// named entry point so that each component gets a dedicated output chunk.
// In production a single app.tsx entry point is sufficient.
const componentEntries = watchMode
  ? [
      ...collectTsxFiles(resolve(sourceDir, 'pages')),
      ...collectTsxFiles(resolve(sourceDir, 'components')),
    ]
  : [];

const entryPoints = [resolve(sourceDir, 'app.tsx'), ...componentEntries];

// Build a map of original basename -> stable basename for this build's outputs.
//
// esbuild appends an 8-char content hash to every dynamically-split chunk,
// e.g.  Index-DFFMCZIQ.js  ->  Index.js
//       chunk-RP6ZAXYD.js  ->  chunk.js
//
// Multiple auto-generated shared chunks can produce the same stable name
// (all map to "chunk.js").  Those keep their hashes to avoid collision;
// only uniquely-resolvable names get de-hashed.
function buildStableNameMap(outputs) {
  const counts  = new Map(); // stable basename -> occurrence count
  const entries = [];

  for (const outfile of outputs) {
    const orig   = basename(outfile);
    const stable = orig.replace(/-[A-Za-z0-9]{8}(?=\.js$)/, '');
    entries.push({ orig, stable });
    counts.set(stable, (counts.get(stable) ?? 0) + 1);
  }

  const map = new Map(); // orig -> final name
  for (const { orig, stable } of entries) {
    map.set(orig, counts.get(stable) === 1 ? stable : orig);
  }

  return map;
}

const deployPlugin = {
  name: 'deploy',
  setup(build) {
    build.onEnd(result => {
      if (result.errors.length === 0 && result.metafile) {
        const outputs  = Object.keys(result.metafile.outputs);
        const nameMap  = buildStableNameMap(outputs);

        // Collect only the replacements that actually de-hash a name.
        const replacements = [];
        for (const [orig, stable] of nameMap) {
          if (orig !== stable) replacements.push([orig, stable]);
        }

        for (const outfile of outputs) {
          const abs      = resolve(ROOT, outfile);
          const destName = nameMap.get(basename(outfile));
          const dest     = resolve(publicDir, destName);

          // Read output, replace all hash-named references with stable names.
          let content = readFileSync(abs, 'utf8');
          for (const [orig, stable] of replacements) {
            content = content.replaceAll(orig, stable);
          }

          // Only write when content changed -- NeonSignal fires HMR only for
          // files that are actually overwritten (existing files, "modified").
          // New files created here on first build are fine; subsequent edits
          // overwrite them, keeping the filename stable across rebuilds.
          mkdirSync(dirname(dest), { recursive: true });
          let existing = null;
          try { existing = readFileSync(dest, 'utf8'); } catch { /* new file */ }
          if (existing !== content) {
            writeFileSync(dest, content);
          }
        }

        // Remove stale chunk files from previous builds.
        const currentDestNames = new Set();
        for ( const outfile of outputs ) {
          currentDestNames.add( nameMap.get( basename( outfile ) ) );
        }

        const hashPattern = /-[A-Za-z0-9]{8}\.js$/;
        let existingJsFiles;
        try { existingJsFiles = readdirSync( publicDir ); }
        catch { existingJsFiles = []; }
        for ( const file of existingJsFiles ) {
          if ( file.endsWith( '.js' ) && hashPattern.test( file ) && !currentDestNames.has( file ) ) {
            try { unlinkSync( resolve( publicDir, file ) ); }
            catch { /* ignore */ }
          }
        }

        console.log(`[ok] ${watchMode ? '[watch] rebuilt' : 'build complete'} -> ${publicDir}`);
      }
    });
  },
};

console.log(`[>] phosphor website build  watch=${watchMode}`);

const ctx = await esbuild.context({
  entryPoints,
  bundle:      true,
  minify:      !watchMode,
  format:      'esm',
  platform:    'browser',
  target:      'es2020',
  jsx:         'transform',
  jsxFactory:  'h',
  jsxFragment: 'Fragment',
  define: {
    __PHOSPHOR_DEV__:        JSON.stringify(watchMode),
    __PHOSPHOR_PUBLIC_DIR__: JSON.stringify(publicDir),
  },
  outdir:      buildDir,
  splitting:   true,
  entryNames:  '[name]',
  metafile:    true,
  plugins:     [
    // cathodeHMRPlugin auto-wraps all PascalCase component exports with
    // hmr.wrap() so that each component registers itself in the HMR
    // registry without any per-file boilerplate.  Production builds
    // skip this plugin entirely.
    ...(watchMode ? [cathodeHMRPlugin()] : []),
    deployPlugin,
  ],
});

if (watchMode) {
  await ctx.watch();
  console.log('[>] Watching for changes  (Ctrl-C to stop)');
  const stop = () => { ctx.dispose(); process.exit(0); };
  process.on('SIGINT', stop);
  process.on('SIGTERM', stop);
} else {
  await ctx.rebuild();
  await ctx.dispose();
}
