#!/usr/bin/env node
// Explicitly local wrapper for the optional Khronos validator, installed separately.
const fs = require('node:fs');
const path = require('node:path');
const { parseArgs } = require('node:util');
const { values } = parseArgs({ options: {
  validator: { type: 'string' }, asset: { type: 'string' }, output: { type: 'string' },
  'resource-root': { type: 'string' }, help: { type: 'boolean' }
} });
if (values.help) {
  console.log('node validate_gltf.cjs --validator PATH_TO_PACKAGE --asset FILE.gltf --output REPORT.json [--resource-root DIR]');
  process.exit(0);
}
for (const key of ['validator', 'asset', 'output']) {
  if (!values[key]) throw new Error(`Missing --${key}`);
}
const validator = require(path.resolve(values.validator));
if (validator.version() !== '2.0.0-dev.3.10') throw new Error('Expected pinned validator 2.0.0-dev.3.10');
const asset = path.resolve(values.asset);
const root = fs.realpathSync(values['resource-root'] || path.dirname(asset));
const cap = 64 * 1024 * 1024;
function readBounded(file) {
  if (fs.statSync(file).size > cap) throw new Error('Local validation input exceeds 64 MiB');
  return fs.readFileSync(file);
}
validator.validateBytes(new Uint8Array(readBounded(asset)), {
  uri: path.basename(asset), maxIssues: 100, writeTimestamp: false,
  externalResourceFunction: async uri => {
    if (/^[a-z][a-z0-9+.-]*:/i.test(uri)) throw new Error('External resource must be a local relative file');
    const resolved = fs.realpathSync(path.resolve(path.dirname(asset), decodeURIComponent(uri)));
    const relative = path.relative(root, resolved);
    if (relative === '..' || relative.startsWith('..' + path.sep) || path.isAbsolute(relative)) {
      throw new Error('External resource escapes explicit resource root');
    }
    return new Uint8Array(readBounded(resolved));
  }
}).then(report => {
  fs.writeFileSync(values.output, JSON.stringify(report, null, 2) + '\n', { flag: 'wx' });
  console.log(JSON.stringify({ validator: validator.version(), ...report.issues, output: path.resolve(values.output) }));
  process.exitCode = report.issues.numErrors ? 1 : 0;
}).catch(error => { console.error(error); process.exitCode = 1; });
