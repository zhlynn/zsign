// Smoke checks for the packaged CLI and module API.
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import fs from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { mapArguments } from './node-cli.cjs';

const repositoryRoot = fileURLToPath(new URL('../../', import.meta.url));
const runtimeDirectory = path.resolve(process.env.ZSIGN_WASM_DIR ?? path.join(repositoryRoot, 'bin'));
const cli = path.join(runtimeDirectory, 'zsign.js');
const fixture = await fs.readFile(path.join(repositoryRoot, 'test/dylib/bin/demo1.dylib'));

// Signature verification.
const MACHO_64_MAGIC = 0xfeedfacf;
const LC_CODE_SIGNATURE = 0x1d;
const SUPERBLOB_MAGIC = 0xfade0cc0;
const CODE_DIRECTORY_MAGIC = 0xfade0c02;
const AD_HOC_FLAG = 2;
const HASH_TYPES = new Map([
  [1, { algorithm: 'SHA-1', size: 20 }],
  [2, { algorithm: 'SHA-256', size: 32 }],
  [3, { algorithm: 'SHA-256', size: 20 }],
]);

function findSignature(view) {
  assert.ok(view.byteLength >= 32, 'Mach-O header is truncated');
  assert.ok(view.getUint32(0, true) === MACHO_64_MAGIC, 'Expected a little-endian 64-bit Mach-O');

  const commandCount = view.getUint32(16, true);
  const commandsEnd = 32 + view.getUint32(20, true);
  assert.ok(commandsEnd <= view.byteLength, 'Mach-O load commands exceed the file size');

  let command = 32;
  for (let index = 0; index < commandCount; index++) {
    assert.ok(command + 8 <= commandsEnd, `Load command ${index} is truncated`);
    const type = view.getUint32(command, true);
    const size = view.getUint32(command + 4, true);
    assert.ok(size >= 8 && command + size <= commandsEnd, `Load command ${index} has an invalid size`);

    if (type === LC_CODE_SIGNATURE) {
      assert.ok(size >= 16, 'LC_CODE_SIGNATURE is truncated');
      const offset = view.getUint32(command + 8, true);
      const length = view.getUint32(command + 12, true);
      assert.ok(offset > 0 && length >= 12, 'Code signature is empty');
      assert.ok(offset + length <= view.byteLength, 'Code signature exceeds the file size');
      return { offset, length };
    }
    command += size;
  }
  throw new Error('Mach-O has no LC_CODE_SIGNATURE command');
}

async function verifyCodeDirectory(bytes, view, signature, offset, signatureLength) {
  const start = signature.offset + offset;
  const size = view.getUint32(start + 4);
  assert.ok(size >= 44 && offset + size <= signatureLength, 'CodeDirectory has an invalid size');

  const flags = view.getUint32(start + 12);
  const hashOffset = view.getUint32(start + 16);
  const slotCount = view.getUint32(start + 28);
  const codeLimit = view.getUint32(start + 32);
  const hashSize = view.getUint8(start + 36);
  const hashType = view.getUint8(start + 37);
  const pageLog = view.getUint8(start + 39);
  const hash = HASH_TYPES.get(hashType);

  assert.ok(flags & AD_HOC_FLAG, 'Expected an ad-hoc CodeDirectory');
  assert.ok(codeLimit === signature.offset, 'CodeDirectory does not cover the entire unsigned image');
  assert.ok(pageLog <= 20, 'CodeDirectory page size is too large');
  assert.ok(hash && hashSize === hash.size, `Unsupported CodeDirectory hash type or size: ${hashType}/${hashSize}`);

  const pageSize = 2 ** pageLog;
  assert.ok(slotCount === Math.ceil(codeLimit / pageSize), 'CodeDirectory has an incorrect page count');
  assert.ok(hashOffset >= 44, 'CodeDirectory hashes overlap its header');
  assert.ok(hashOffset + slotCount * hashSize <= size, 'CodeDirectory hashes exceed its size');

  for (let page = 0; page < slotCount; page++) {
    const pageStart = page * pageSize;
    const pageEnd = Math.min(codeLimit, pageStart + pageSize);
    const digest = new Uint8Array(await crypto.subtle.digest(
      hash.algorithm,
      bytes.subarray(pageStart, pageEnd),
    ));
    const storedHash = start + hashOffset + page * hashSize;
    const expected = bytes.subarray(storedHash, storedHash + hashSize);
    assert.ok(expected.every((byte, index) => byte === digest[index]), `CodeDirectory page ${page} hash mismatch`);
  }
  return slotCount;
}

async function verifySignature(bytes) {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const signature = findSignature(view);
  const start = signature.offset;
  assert.ok(view.getUint32(start) === SUPERBLOB_MAGIC, 'Invalid code signature SuperBlob');

  const length = view.getUint32(start + 4);
  const count = view.getUint32(start + 8);
  const indexEnd = 12 + count * 8;
  assert.ok(length <= signature.length, 'SuperBlob exceeds the signature allocation');
  assert.ok(indexEnd <= length, 'SuperBlob index is truncated');

  let directories = 0;
  let pages = 0;
  for (let index = 0; index < count; index++) {
    const offset = view.getUint32(start + 16 + index * 8);
    assert.ok(offset >= indexEnd && offset + 8 <= length, `SuperBlob entry ${index} is out of bounds`);
    if (view.getUint32(start + offset) !== CODE_DIRECTORY_MAGIC) continue;

    pages += await verifyCodeDirectory(bytes, view, signature, offset, length);
    directories++;
  }
  assert.ok(directories > 0 && pages > 0, 'Signature contains no code page hashes');
  return { directories, pages };
}

// CLI checks.
function testArgumentMapping() {
  const mapPath = value => value.startsWith('/') ? '/host' + value : value;
  assert.deepEqual(
    mapArguments([
      '-qfk/key with spaces.p12',
      '-p', '/password',
      '--bundle_id=/literal',
      '--prov=/profile',
      '-o', '/output.ipa',
      '--', '/input.ipa',
    ], mapPath),
    [
      '-qfk/host/key with spaces.p12',
      '-p', '/password',
      '--bundle_id=/literal',
      '--prov=/host/profile',
      '-o', '/host/output.ipa',
      '--', '/host/input.ipa',
    ],
    'Path arguments should be mapped without changing passwords or bundle identifiers',
  );
  assert.deepEqual(
    mapArguments([
      '--password=/unchanged',
      '-p/unchanged',
      '--bundle_version', '/unchanged',
      '-k', 'relative.p12',
      'relative.app',
    ], mapPath),
    [
      '--password=/unchanged',
      '-p/unchanged',
      '--bundle_version', '/unchanged',
      '-k', 'relative.p12',
      'relative.app',
    ],
    'Scalar option values should remain unchanged',
  );
}

function runCli(args, cwd, expectedStatus = 0) {
  const result = spawnSync(process.execPath, [cli, ...args], {
    cwd,
    encoding: 'utf8',
    timeout: 60000,
  });
  assert.ifError(result.error);
  assert.equal(result.status, expectedStatus, result.stdout + result.stderr);
}

async function verifySignedFile(file) {
  await verifySignature(new Uint8Array(await fs.readFile(file)));
}

function assertZip(bytes) {
  assert.ok(bytes.byteLength >= 4, 'IPA output is truncated');
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  assert.equal(view.getUint32(0, true), 0x04034b50, 'IPA output has no ZIP header');
}

async function testFilePaths(directory) {
  for (const absolute of [false, true]) {
    const name = `${absolute ? 'absolute' : 'relative'} fixture.dylib`;
    const file = path.join(directory, name);
    await fs.writeFile(file, fixture);
    runCli(['-qaf', '--', absolute ? file : name], directory);
    await verifySignedFile(file);
  }
  runCli(['-a', path.join(directory, 'missing')], directory, 255);
}

async function testSymlinks(directory) {
  // Creating symlinks can require additional privileges on Windows.
  if (process.platform === 'win32') return;

  const target = path.join(directory, 'target.dylib');
  const links = [
    { name: 'relative-link', destination: 'target.dylib' },
    { name: 'absolute-link', destination: target },
  ];
  for (const { name, destination } of links) {
    const link = path.join(directory, name);
    await fs.symlink(destination, link);
    await fs.writeFile(target, fixture);
    runCli(['-qaf', '--', link], directory);
    await verifySignedFile(target);
  }

  const nestedDirectory = path.join(directory, 'nested');
  await fs.mkdir(path.join(nestedDirectory, 'child'), { recursive: true });
  await fs.symlink(path.join(nestedDirectory, 'child'), path.join(directory, 'directory-link'));
  const nestedTarget = path.join(nestedDirectory, 'target.dylib');
  await fs.writeFile(nestedTarget, fixture);

  // path.join() would normalize away the symlink/.. sequence under test.
  runCli(['-qaf', '--', `${directory}/directory-link/../target.dylib`], directory);
  await verifySignedFile(nestedTarget);

  await fs.writeFile(target, fixture);
  runCli(['-qaf', '--', '../../' + target.slice(1)], '/');
  await verifySignedFile(target);
}

async function testIpa(directory) {
  const app = path.join(directory, 'source', 'Payload', 'Smoke app.app');
  await fs.mkdir(app, { recursive: true });
  await fs.writeFile(path.join(app, 'Smoke'), fixture);
  await fs.writeFile(path.join(app, 'Info.plist'), `<?xml version="1.0"?>
<plist version="1.0">
<dict>
  <key>CFBundleIdentifier</key><string>com.example.smoke</string>
  <key>CFBundleExecutable</key><string>Smoke</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleVersion</key><string>1</string>
  <key>SmokeLargeInteger</key><integer>4294967297</integer>
</dict>
</plist>
`);

  let output = path.join(directory, 'signed output.ipa');
  if (process.platform !== 'win32') {
    const resource = path.join(app, 'resource.txt');
    await fs.writeFile(resource, 'resource');
    await fs.symlink(resource, path.join(app, 'resource-link.txt'));

    const outputDirectory = path.join(directory, 'output', 'child');
    await fs.mkdir(outputDirectory, { recursive: true });
    await fs.symlink(outputDirectory, path.join(directory, 'output-link'));
    output = `${directory}/output-link/../signed output.ipa`;
  }

  runCli(['-qaf', '--bundle_version=2', '--output=' + output, '--', app], directory);
  const plist = await fs.readFile(path.join(app, 'Info.plist'), 'utf8');
  assert.match(plist, /<integer>4294967297<\/integer>/, 'Signing truncated a 64-bit plist integer');
  await verifySignedFile(path.join(app, 'Smoke'));
  const ipa = await fs.readFile(output);
  assertZip(ipa);

  const temp = path.join(directory, 'temp');
  const resigned = path.join(directory, 'resigned.ipa');
  await fs.mkdir(temp);
  runCli(['-qaf', '-t', temp, '-o', resigned, output], directory);
  assert.deepEqual(await fs.readdir(temp), [], 'IPA extraction left temporary files');
  assertZip(await fs.readFile(resigned));
  return ipa;
}

async function testHostFilesystem() {
  const directory = await fs.mkdtemp(path.join(tmpdir(), 'zsign smoke '));
  try {
    await testFilePaths(directory);
    await testSymlinks(directory);
    return await testIpa(directory);
  } finally {
    await fs.rm(directory, { recursive: true, force: true });
  }
}

// Module API checks.
function testModuleLifecycle(module) {
  const openStreams = () => module.FS.streams.filter(Boolean).length;
  const baseline = openStreams();
  const versionArgs = ['-v'];

  // This exceeds the number of calls that previously exhausted the WASM stack.
  for (let index = 0; index < 5000; index++) {
    assert.equal(module.callMain(versionArgs), 0, `Version call ${index + 1} failed`);
  }
  assert.deepEqual(versionArgs, ['-v'], "callMain mutated the caller's arguments");

  module.FS.writeFile('/inspect.dylib', new Uint8Array(fixture));
  module.FS.writeFile('/invalid.dylib', new Uint8Array(32));
  for (let index = 0; index < 100; index++) {
    assert.equal(module.callMain(['-qaf', '/invalid.dylib']), -1, 'Invalid binary was accepted');
    assert.equal(openStreams(), baseline, 'Failed signing leaked a file stream');
    assert.equal(module.callMain(['-q', '/inspect.dylib']), 0, 'Binary inspection failed');
    assert.equal(openStreams(), baseline, 'Binary inspection leaked a file stream');
  }
}

async function testModuleApi(ipa) {
  const moduleUrl = pathToFileURL(path.join(runtimeDirectory, 'zsign.mjs'));
  const { default: createZsignModule } = await import(moduleUrl.href);
  const logs = [];
  const module = await createZsignModule({
    wasmBinary: await fs.readFile(path.join(runtimeDirectory, 'zsign.wasm')),
    noInitialRun: true,
    print: text => logs.push(text),
    printErr: text => logs.push(text),
    stdin: () => null,
  });
  const cases = [
    { args: ['-qaf'], directories: 1 },
    { args: ['--adhoc', '--force', '--legacy_sha1'], directories: 2 },
  ];

  for (const [index, testCase] of cases.entries()) {
    const file = `/fixture${index}.dylib`;
    // Use Uint8Array so MEMFS does not retain a Node Buffer slice of the fixture.
    module.FS.writeFile(file, new Uint8Array(fixture));
    const status = module.callMain([...testCase.args, file]);
    assert.equal(status, 0, `Module signing call ${index + 1} failed`);

    const output = module.FS.readFile(file);
    assert.notDeepEqual(output, new Uint8Array(fixture), 'Module signing left the fixture unchanged');
    const signature = await verifySignature(output);
    assert.equal(signature.directories, testCase.directories, 'Module did not apply its hash options');
    if (index === 0) assert.equal(logs.length, 0, 'Quiet mode did not suppress output');
  }
  assert.ok(logs.length > 0, 'Quiet mode persisted into the second module call');

  module.FS.writeFile('/input.ipa', new Uint8Array(ipa));
  assert.equal(module.callMain(['-qaf', '-o', '/signed.ipa', '/input.ipa']), 0, 'Module IPA signing failed');
  assertZip(module.FS.readFile('/signed.ipa'));
  const leftovers = module.FS.readdir('/tmp').filter(name => name.startsWith('zsign_folder_'));
  assert.deepEqual(leftovers, [], 'Module IPA extraction left temporary files');
  testModuleLifecycle(module);
}

// Run the smoke checks.
testArgumentMapping();
const ipa = await testHostFilesystem();
await testModuleApi(ipa);
console.log('WASM smoke tests passed: CLI paths, signatures, IPA, plist integers and repeated module calls.');
