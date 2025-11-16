#!/usr/bin/env node

const https = require('https');
const fs = require('fs');
const path = require('path');
const tar = require('tar');

const version = require('../package.json').version;
const platform = process.platform;
const arch = process.arch;
const abi = process.versions.modules;

const prebuildName = `pgnx-v${version}-node-v${abi}-${platform}-${arch}.tar.gz`;
const releaseUrl = `https://github.com/Lumos-Labs-HQ/pgnx/releases/download/v${version}/${prebuildName}`;
const buildDir = path.join(__dirname, '..', 'build', 'Release');

console.log(`🔍 Downloading prebuild: ${prebuildName}`);

function download(url) {
  https.get(url, (res) => {
    if (res.statusCode === 302 || res.statusCode === 301) {
      download(res.headers.location);
      return;
    }
    
    if (res.statusCode === 200) {
      console.log('✅ Found, downloading...');
      
      const tarPath = path.join(__dirname, '..', prebuildName);
      const file = fs.createWriteStream(tarPath);
      
      res.pipe(file);
      
      file.on('finish', () => {
        file.close();
        console.log('📦 Extracting...');
        
        fs.mkdirSync(buildDir, { recursive: true });
        
        tar.extract({
          file: tarPath,
          cwd: buildDir,
          sync: true
        });
        
        fs.unlinkSync(tarPath);
        console.log('✅ Installation complete!\n');
        process.exit(0);
      });
    } else {
      console.error(`\n❌ Prebuild not available for your system!`);
      console.error(`Platform: ${platform}, Arch: ${arch}, Node: v${abi}`);
      console.error(`\nPlease report this at: https://github.com/Lumos-Labs-HQ/pgnx/issues\n`);
      process.exit(1);
    }
  }).on('error', (err) => {
    console.error('\n❌ Download failed:', err.message);
    console.error('Please check your internet connection or report at:');
    console.error('https://github.com/Lumos-Labs-HQ/pgnx/issues\n');
    process.exit(1);
  });
}

download(releaseUrl);
