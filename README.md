# PGN - High-Performance PostgreSQL Driver

A high-performance Node.js PostgreSQL driver written in C++ using libpqxx and N-API.

## Features

- **Connection Pool**: C++ implemented connection pool for efficient resource management
- **Async Architecture**: Fully asynchronous, non-blocking operations
- **Query Pipelining**: Batch multiple queries using pqxx::pipeline
- **Prepared Statements**: Named prepared statements with parameter binding
- **Binary Mode**: Optimized result conversion with zero-copy buffers
- **LISTEN/NOTIFY**: Background thread-based pub/sub with ThreadSafeFunction
- **Type Support**: Automatic type conversion (int, bool, bytea, text)

## Installation

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install libpqxx-dev postgresql-server-dev-all

# Install Node.js dependencies
npm install
```

## Usage

```javascript
const { Connection } = require('pgn-driver');

const conn = new Connection('postgresql://user:pass@localhost/db', 10);

// Query with parameters
const rows = await conn.query('SELECT * FROM users WHERE id = $1', [1]);

// Prepared statements
conn.prepare('getUser', 'SELECT * FROM users WHERE id = $1');
const user = await conn.execute('getUser', [1]);

// Pipeline multiple queries
const results = await conn.pipeline([
  'INSERT INTO logs (msg) VALUES (\'log1\')',
  'INSERT INTO logs (msg) VALUES (\'log2\')'
]);

// LISTEN/NOTIFY
conn.listen('channel', (payload) => {
  console.log('Received:', payload);
});

conn.unlisten('channel');
conn.close();
```

## API

### `new Connection(connectionString, poolSize)`
Create a new connection pool.

### `query(sql, params?): Promise<Array>`
Execute a query with optional parameters.

### `prepare(name, sql): void`
Prepare a named statement.

### `execute(name, params?): Promise<Array>`
Execute a prepared statement.

### `pipeline(queries): Promise<Array>`
Execute multiple queries in a pipeline.

### `listen(channel, callback): void`
Listen for notifications on a channel.

### `unlisten(channel): void`
Stop listening on a channel.

### `close(): void`
Close all connections.
