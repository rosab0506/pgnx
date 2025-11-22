#!/usr/bin/env node

const fs = require('fs');
const path = require('path');
const tar = require('tar');

const platform = process.platform;
const arch = process.arch;
const abi = process.versions.modules;

const prebuildName = `pgnx-node-v${abi}-${platform}-${arch}.tar.gz`;
const prebuildsDir = path.join(__dirname, '..', 'prebuilds');
const prebuildPath = path.join(prebuildsDir, prebuildName);
const buildDir = path.join(__dirname, '..', 'build', 'Release');

if (fs.existsSync(prebuildPath)) {
  console.log(`📦 Extracting prebuild for ${platform}-${arch} (Node ABI ${abi})...`);

  fs.mkdirSync(buildDir, { recursive: true });
  tar.extract({ file: prebuildPath, cwd: buildDir, sync: true });

  console.log('🧹 Cleaning up unused prebuilds...');
  if (fs.existsSync(prebuildsDir)) {
    const files = fs.readdirSync(prebuildsDir);
    let deletedCount = 0;
    files.forEach(file => {
      const filePath = path.join(prebuildsDir, file);
      try {
        fs.unlinkSync(filePath);
        deletedCount++;
      } catch (err) {
      }
    });

    try {
      fs.rmdirSync(prebuildsDir);
      console.log(`✅ Removed ${deletedCount} unused prebuilds. Installation complete!`);
    } catch (err) {
      console.log(`✅ Cleaned up ${deletedCount} unused prebuilds. Installation complete!`);
    }
  }
} else {
  console.error(`❌ Prebuild not available for: ${platform}-${arch} (Node ABI ${abi})`);
  console.error('Available prebuilds:');
  if (fs.existsSync(prebuildsDir)) {
    fs.readdirSync(prebuildsDir).forEach(file => console.error(`  - ${file}`));
  }
  console.error('Please report at: https://github.com/Lumos-Labs-HQ/pgnx/issues');
  process.exit(1);
}
