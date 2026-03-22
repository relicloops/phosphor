#!/usr/bin/env node
import * as esbuild                                                    from 'esbuild';
import { cathodeHMRPlugin }                                            from '@relicloops/cathode/esbuild-plugin';
import { cpSync, mkdirSync, existsSync, readFileSync, readdirSync,
         writeFileSync, unlinkSync }                                   from 'node:fs';
import { resolve, dirname, basename }                                  from 'node:path';
import { fileURLToPath }                                               from 'node:url';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '../..');

// Defaults
let sourceDir = process.env.PROJECT_SOURCE_DIR ?? resolve(ROOT, 'src');
let buildDir  = process.env.PROJECT_BUILD_DIR  ?? resolve(ROOT, 'build/src');
let publicDir = process.env.PROJECT_PUBLIC_DIR ?? resolve(ROOT, 'public/_default');
let watchMode = false;

const args = process.argv.slice(2);
for (let i = 0; i < args.length; i++) {
  switch (args[i]) {
    case '--build':          buildDir  = args[++i]; break;
    case '--public':
    case '--deploy':         publicDir = args[++i]; break;
    case '--watch':          watchMode = true;      break;
    case '--help': case '-h':
      console.log('Usage: node build.mjs [--build <dir>] [--public <dir>] [--watch]');
      process.exit(0);
  }
}

// Static assets are deployed to publicDir once before the build context starts.
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
function collectTsxFiles(dir) {
  const results = [];
  let entries;
  try { entries = readdirSync(dir, { withFileTypes: true }); }
  catch { return results; }
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

const componentEntries = watchMode
  ? [
      ...collectTsxFiles(resolve(sourceDir, 'pages')),
      ...collectTsxFiles(resolve(sourceDir, 'components')),
    ]
  : [];

const entryPoints = [resolve(sourceDir, 'app.tsx'), ...componentEntries];

function buildStableNameMap(outputs) {
  const counts  = new Map();
  const entries = [];

  for (const outfile of outputs) {
    const orig   = basename(outfile);
    const stable = orig.replace(/-[A-Za-z0-9]{8}(?=\.js$)/, '');
    entries.push({ orig, stable });
    counts.set(stable, (counts.get(stable) ?? 0) + 1);
  }

  const map = new Map();
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

        const replacements = [];
        for (const [orig, stable] of nameMap) {
          if (orig !== stable) replacements.push([orig, stable]);
        }

        for (const outfile of outputs) {
          const abs      = resolve(ROOT, outfile);
          const destName = nameMap.get(basename(outfile));
          const dest     = resolve(publicDir, destName);

          let content = readFileSync(abs, 'utf8');
          for (const [orig, stable] of replacements) {
            content = content.replaceAll(orig, stable);
          }

          mkdirSync(dirname(dest), { recursive: true });
          let existing = null;
          try { existing = readFileSync(dest, 'utf8'); } catch { /* new file */ }
          if (existing !== content) {
            writeFileSync(dest, content);
          }
        }

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

console.log(`[>] <<name>> build  watch=${watchMode}`);

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
    __PROJECT_DEV__:        JSON.stringify(watchMode),
    __PROJECT_PUBLIC_DIR__: JSON.stringify(publicDir),
  },
  outdir:      buildDir,
  splitting:   true,
  entryNames:  '[name]',
  metafile:    true,
  plugins:     [
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
