#!/usr/bin/env node

const { execSync } = require('child_process');

console.log('⚠️  Prebuilt binary not available, building from source...\n');

try {
  execSync('node-gyp rebuild', { stdio: 'inherit' });
  console.log('\n✅ Build successful!\n');
} catch (error) {
  console.error('\n❌ Build failed!');
  console.error('Please install build dependencies:');
  console.error('Linux: sudo apt-get install -y libpqxx-dev postgresql-server-dev-all build-essential');
  console.error('macOS: brew install libpqxx postgresql');
  console.error('Windows: See https://github.com/Lumos-Labs-HQ/pgnx#readme\n');
  process.exit(1);
}
