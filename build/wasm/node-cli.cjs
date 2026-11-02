// Host-filesystem CLI for the same module/WASM used by browsers. Node built-ins only.
const path = require('node:path');
// Keep value/path options aligned with getopt_long in src/zsign.cpp.
const pathOptions = new Set('ckmoeIltx');
const valueOptions = new Set('ckmopebnzlDtrxMI');
const options = {
  debug: 'd', force: 'f', version: 'v', adhoc: 'a', cert: 'c', pkey: 'k', prov: 'm',
  password: 'p', bundle_id: 'b', bundle_name: 'n', bundle_version: 'r', entitlements: 'e',
  icon: 'I', output: 'o', zip_level: 'z', dylib: 'l', rm_dylib: 'D', weak: 'w',
  temp_folder: 't', sha256_only: '2', legacy_sha1: 'L', install: 'i', check: 'C',
  quiet: 'q', metadata: 'x', rm_provision: 'R', enable_docs: 'S', min_version: 'M',
  rm_extensions: 'E', rm_watch: 'W', rm_uisd: 'U', inject_extensions: 'P', help: 'h',
};
function mapArguments(args, mapPath) {
  const output = [];
  let positional = false;
  for (let i = 0; i < args.length; i++) {
    const arg = args[i];
    if (positional || !arg.startsWith('-') || arg === '-') {
      output.push(mapPath(arg));
      continue;
    }
    if (arg === '--') {
      positional = true;
      output.push(arg);
      continue;
    }
    if (arg.startsWith('--')) {
      const equal = arg.indexOf('='), name = arg.slice(2, equal < 0 ? undefined : equal);
      const matches = Object.keys(options).filter(k => k.startsWith(name));
      const option = options[name] ?? (matches.length === 1 ? options[matches[0]] : undefined);
      if (!valueOptions.has(option)) {
        output.push(arg);
        continue;
      }
      if (equal >= 0) {
        const value = arg.slice(equal + 1);
        output.push(arg.slice(0, equal + 1) + (pathOptions.has(option) ? mapPath(value) : value));
      } else {
        output.push(arg);
        if (i + 1 < args.length) {
          const value = args[++i];
          output.push(pathOptions.has(option) ? mapPath(value) : value);
        }
      }
      continue;
    }
    let rewritten = arg;
    for (let j = 1; j < arg.length; j++) {
      if (!valueOptions.has(arg[j])) continue;
      if (j + 1 < arg.length) {
        const value = arg.slice(j + 1);
        rewritten = arg.slice(0, j + 1) + (pathOptions.has(arg[j]) ? mapPath(value) : value);
      } else if (i + 1 < args.length) {
        output.push(arg);
        const value = args[++i];
        rewritten = pathOptions.has(arg[j]) ? mapPath(value) : value;
      }
      break;
    }
    output.push(rewritten);
  }
  return output;
}
exports.mapArguments = mapArguments;
if (require.main === module) (async () => {
  const fs = require('node:fs'), { pathToFileURL } = require('node:url');
  const { default: create } = await import(pathToFileURL(path.join(__dirname, 'zsign.mjs')).href);
  const module = await create({
    wasmBinary: fs.readFileSync(path.join(__dirname, 'zsign.wasm')),
    noInitialRun: true, noExitRuntime: true,
  });
  const roots = new Map();
  const realpath = fs.realpathSync.native ?? fs.realpathSync;
  const hostPath = value => {
    if (value === '' || value === '-') return value;
    // Resolve components before '..'; some runtimes normalize realpath inputs first.
    const absolute = path.isAbsolute(value) ? value : process.cwd() + path.sep + value;
    const rootPath = path.parse(absolute).root;
    const parts = absolute.slice(rootPath.length).split(path.sep);
    value = rootPath;
    for (let i = 0; i < parts.length; i++) {
      if (!parts[i] || parts[i] === '.') continue;
      if (parts[i] === '..') { value = path.dirname(value); continue; }
      const next = path.join(value, parts[i]);
      try { value = realpath(next); } catch {
        // Preserve missing components so an invalid parent path still fails in NODEFS.
        value = next + (i + 1 < parts.length ? path.sep + parts.slice(i + 1).join(path.sep) : '');
        break;
      }
    }
    const root = path.parse(value).root;
    if (!roots.has(root)) {
      const mount = '/host' + (roots.size || '');
      module.FS.mkdir(mount);
      module.FS.mount(module.NODEFS, { root }, mount);
      roots.set(root, mount);
    }
    return roots.get(root) + '/' + value.slice(root.length).split(path.sep).join('/');
  };
  // NODEFS returns host symlink targets; translate absolute targets into the mount.
  const readlink = module.NODEFS.node_ops.readlink;
  module.NODEFS.node_ops.readlink = node => {
    const target = readlink(node);
    return path.isAbsolute(target) ? hostPath(target) : target.split(path.sep).join('/');
  };
  module.FS.chdir(hostPath(process.cwd()));
  const temp = process.platform === 'win32' ? require('node:os').tmpdir() : '/tmp';
  const args = ['-t', hostPath(temp), ...mapArguments(process.argv.slice(2), hostPath)];
  try {
    process.exitCode = module.callMain(args) & 255;
  } finally {
    args.fill('');
    module.HEAPU8.fill(0);
  }
})().catch(error => {
  console.error('zsign: unable to run shared WASM CLI:', error.message);
  process.exitCode = 1;
});
