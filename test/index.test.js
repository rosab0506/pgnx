const assert = require('assert');
const { Connection } = require('../index');

const connStr = process.env.DATABASE_URL || 'postgresql://postgres:postgres@localhost:5432/testdb';

let passed = 0;
let failed = 0;

function test(name, fn) {
    return fn().then(() => {
        console.log(`  PASS: ${name}`);
        passed++;
    }).catch((err) => {
        console.error(`  FAIL: ${name} - ${err.message}`);
        failed++;
    });
}

async function runTests() {
    console.log('Running PGNX Tests...\n');

    const conn = new Connection(connStr, 5);

    try {
        // --- Basic queries ---

        await test('Basic query', async () => {
            const result = await conn.query('SELECT 1 as num');
            assert.strictEqual(result[0].num, 1);
        });

        await test('Synchronous query', async () => {
            const result = conn.querySync('SELECT 42 as answer');
            assert.strictEqual(result[0].answer, 42);
        });

        // --- Table setup ---

        await test('Create table', async () => {
            await conn.query('DROP TABLE IF EXISTS test_users');
            await conn.query('CREATE TABLE test_users (id SERIAL PRIMARY KEY, name TEXT, age INT, active BOOLEAN DEFAULT true)');
        });

        // --- Parameterized queries ---

        await test('Insert with parameters', async () => {
            await conn.query('INSERT INTO test_users (name, age) VALUES ($1, $2)', ['Alice', 25]);
            await conn.query('INSERT INTO test_users (name, age) VALUES ($1, $2)', ['Bob', 30]);
        });

        await test('Query with parameters', async () => {
            const users = await conn.query('SELECT * FROM test_users WHERE age > $1', [20]);
            assert.strictEqual(users.length, 2);
        });

        await test('Boolean parameter', async () => {
            await conn.query('UPDATE test_users SET active = $1 WHERE name = $2', [false, 'Alice']);
            const result = await conn.query('SELECT active FROM test_users WHERE name = $1', ['Alice']);
            assert.strictEqual(result[0].active, false);
        });

        // --- Prepared statements ---

        await test('Prepared statements', async () => {
            conn.prepare('getUser', 'SELECT * FROM test_users WHERE name = $1');
            const user = await conn.execute('getUser', ['Alice']);
            assert.strictEqual(user[0].name, 'Alice');
        });

        await test('Prepared statement not found', async () => {
            try {
                await conn.execute('nonexistent', []);
                assert.fail('Should have thrown');
            } catch (e) {
                assert.ok(e.message.includes('Prepared statement not found'));
            }
        });

        // --- Pipeline ---

        await test('Pipeline queries', async () => {
            const results = await conn.pipeline([
                "UPDATE test_users SET age = 26 WHERE name = 'Alice'",
                "UPDATE test_users SET age = 31 WHERE name = 'Bob'"
            ]);
            assert.strictEqual(results[0], 1);
            assert.strictEqual(results[1], 1);
        });

        // --- Transactions ---

        await test('Transaction commit', async () => {
            await conn.begin();
            await conn.query("INSERT INTO test_users (name, age) VALUES ($1, $2)", ['Charlie', 35]);
            await conn.commit();
            const result = await conn.query("SELECT * FROM test_users WHERE name = $1", ['Charlie']);
            assert.strictEqual(result.length, 1);
        });

        await test('Transaction rollback', async () => {
            await conn.begin();
            await conn.query("INSERT INTO test_users (name, age) VALUES ($1, $2)", ['Dave', 40]);
            await conn.rollback();
            const result = await conn.query("SELECT * FROM test_users WHERE name = $1", ['Dave']);
            assert.strictEqual(result.length, 0);
        });

        // --- Pool status ---

        await test('Pool status', async () => {
            const status = conn.poolStatus();
            assert.ok(typeof status.available === 'number');
            assert.ok(typeof status.current === 'number');
            assert.ok(typeof status.max === 'number');
            assert.strictEqual(status.max, 5);
            assert.strictEqual(status.closed, false);
        });

        // --- Input validation ---

        await test('Input validation - empty connection string', async () => {
            try {
                new Connection('');
                assert.fail('Should have thrown');
            } catch (e) {
                assert.ok(e.message.length > 0);
            }
        });

        await test('Input validation - invalid pool size', async () => {
            try {
                new Connection(connStr, 0);
                assert.fail('Should have thrown');
            } catch (e) {
                assert.ok(e.message.includes('at least 1'));
            }
        });

        // --- Cleanup ---

        await conn.query('DROP TABLE IF EXISTS test_users');
        conn.close();

    } catch (error) {
        console.error('\nUnexpected error:', error.message);
        try { conn.close(); } catch (e) {}
        process.exit(1);
    }

    console.log(`\nResults: ${passed} passed, ${failed} failed`);
    process.exit(failed > 0 ? 1 : 0);
}

runTests();
