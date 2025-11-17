#!/usr/bin/env node

const fs = require('fs');
const path = require('path');
const tar = require('tar');

const platform = process.platform;
const arch = process.arch;
const abi = process.versions.modules;

const prebuildName = `pgnx-node-v${abi}-${platform}-${arch}.tar.gz`;
const prebuildPath = path.join(__dirname, '..', 'prebuilds', prebuildName);
const buildDir = path.join(__dirname, '..', 'build', 'Release');

if (fs.existsSync(prebuildPath)) {
  console.log('📦 Extracting prebuild...');
  fs.mkdirSync(buildDir, { recursive: true });
  tar.extract({ file: prebuildPath, cwd: buildDir, sync: true });
  console.log('✅ Installation complete!');
} else {
  console.error(`❌ Prebuild not available for: ${platform}-${arch} (Node ABI ${abi})`);
  console.error('Please report at: https://github.com/Lumos-Labs-HQ/pgnx/issues');
  process.exit(1);
}
