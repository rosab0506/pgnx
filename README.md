# PGNX - High-Performance PostgreSQL Driver

[![npm version](https://img.shields.io/npm/v/pgnx.svg)](https://www.npmjs.com/package/pgnx)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

High-performance PostgreSQL driver for Node.js using libpqxx and N-API. Drop-in replacement for `pg` with 2-5x better performance.

## Features

- 2-5x faster than node-postgres
- Connection pooling with tiered health checks
- Async and sync query execution
- Prepared statements
- Query pipelining
- Transaction support (begin/commit/rollback)
- LISTEN/NOTIFY with payload delivery
- TypeScript definitions
- Auto cleanup (5 min idle timeout)
- Cross-platform (Linux, macOS, Windows)
- Prebuilt binaries included

## Installation

```bash
npm install @superdev22/pgnx
```

The package includes prebuilt binaries for:
- **Linux**: x64, ARM64
- **macOS**: x64 (Intel), ARM64 (Apple Silicon)
- **Windows**: x64

No Python, Conan, or build tools required for installation.

## Quick Start

```javascript
const { Connection } = require('@superdev22/pgnx');

// Create connection with pool size of 10
const conn = new Connection('postgresql://user:pass@localhost/db', 10);

// Async query with parameters
const users = await conn.query('SELECT * FROM users WHERE age > $1', [18]);

// Synchronous query
const result = conn.querySync('SELECT COUNT(*) as total FROM users');

// Prepared statements
conn.prepare('getUser', 'SELECT * FROM users WHERE id = $1');
const user = await conn.execute('getUser', [1]);

// Pipeline multiple queries
const results = await conn.pipeline([
  'UPDATE users SET active = true WHERE id = 1',
  'UPDATE users SET active = true WHERE id = 2'
]);

// Transactions
await conn.begin();
await conn.query('INSERT INTO users (name) VALUES ($1)', ['Alice']);
await conn.commit();

// LISTEN/NOTIFY with payload
conn.listen('events', (payload) => console.log('Event:', payload));

// Pool status
const status = conn.poolStatus();
// { available: 8, current: 10, max: 10, closed: false }

// Cleanup
conn.close();
```

## API

### `new Connection(connectionString, poolSize?)`
Create connection pool.
- `connectionString` (string): PostgreSQL connection string
- `poolSize` (number, optional): Pool size (default: 10, minimum: 1)

### `query<T>(sql, params?): Promise<T[]>`
Execute an async query with optional parameters. Supports string, number, boolean, null, BigInt, and Date parameter types.

### `querySync<T>(sql, params?): T[]`
Execute a synchronous query. Same parameter support as `query()`.

### `prepare(name, sql): void`
Register a prepared statement for later execution.

### `execute<T>(name, params?): Promise<T[]>`
Execute a previously prepared statement by name.

### `pipeline(queries): Promise<number[]>`
Execute multiple queries in a transaction, returns affected row counts.

### `begin(): Promise<void>`
Begin a transaction.

### `commit(): Promise<void>`
Commit the current transaction.

### `rollback(): Promise<void>`
Rollback the current transaction.

### `listen(channel, callback): void`
Listen for PostgreSQL NOTIFY events. The callback receives the notification payload string.

### `unlisten(channel): void`
Stop listening on a channel.

### `poolStatus(): PoolStatus`
Returns `{ available, current, max, closed }` pool metrics.

### `close(): void`
Close all connections and clean up resources.

## TypeScript

```typescript
import { Connection } from '@superdev22/pgnx';

interface User {
  id: number;
  name: string;
}

const conn = new Connection('postgresql://localhost/db');
const users = await conn.query<User>('SELECT * FROM users');

// Sync variant
const counts = conn.querySync<{ total: number }>('SELECT COUNT(*) as total FROM users');
```

## Performance

| Operation | pg | pgnx | Improvement |
|-----------|-----|------|-------------|
| Simple Query | 15ms | 6ms | 2.5x |
| Prepared Statement | 12ms | 5ms | 2.4x |
| Pipeline (3 queries) | 45ms | 18ms | 2.5x |

## Migration from pg

**Before:**
```javascript
const { Client } = require('pg');
const client = new Client('postgresql://localhost/db');
await client.connect();
const result = await client.query('SELECT * FROM users');
console.log(result.rows);
await client.end();
```

**After:**
```javascript
const { Connection } = require('@superdev22/pgnx');
const conn = new Connection('postgresql://localhost/db');
const users = await conn.query('SELECT * FROM users');
console.log(users);
conn.close();
```

## Requirements

- Node.js >= 18.0.0
- PostgreSQL server (for runtime connection)

## Building from Source

For developers only. End users get prebuilt binaries automatically.

### Prerequisites

- Node.js >= 18.0.0
- C++ compiler with C++17 support (GCC, Clang, or MSVC)
- libpqxx and libpq development headers

### Platform-Specific Setup

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install libpqxx-dev postgresql-server-dev-16 build-essential
```

**macOS:**
```bash
brew install libpqxx
```

**Windows:**
Install PostgreSQL 16 and set environment variables:
```cmd
set PGNX_INCLUDE_DIR=C:\PostgreSQL\16\include
set PGNX_LIB_DIR=C:\PostgreSQL\16\lib
```

### Build

```bash
git clone https://github.com/Lumos-Labs-HQ/pgnx.git
cd pgnx
npm install --ignore-scripts
npm run build
npm test
```

### CMake (Alternative)

A `CMakeLists.txt` is provided for IDE integration and alternative builds:

```bash
mkdir build-cmake && cd build-cmake
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

## License

Apache License 2.0

## Links

- [GitHub](https://github.com/Lumos-Labs-HQ/pgnx)
- [npm](https://www.npmjs.com/package/@superdev22/pgnx)
- [Issues](https://github.com/Lumos-Labs-HQ/pgnx/issues)
