# PGNX - High-Performance PostgreSQL Driver

## Installation

```bash
npm install pgnx
```

No additional dependencies needed!

## Features

- 🚀 2-5x faster than node-postgres
- 🔄 Connection pooling with health checks
- ⚡ Async operations with Promises
- 🎯 Prepared statements
- 📦 Query pipelining
- 🔔 LISTEN/NOTIFY support
- 🛡️ TypeScript definitions
- ♻️ Auto cleanup (5 min idle timeout)
- 🌍 Cross-platform (Linux, macOS, Windows)

## Quick Start

```javascript
const { Connection } = require('pgnx');

const conn = new Connection('postgresql://user:pass@localhost/db');
const users = await conn.query('SELECT * FROM users');
conn.close();
```

See [README](https://github.com/Lumos-Labs-HQ/pgnx#readme) for full documentation.
