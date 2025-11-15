const { Connection } = require('./index');

const connStr = 'postgresql://neondb_owner:npg_m7N0XzUxPnMu@ep-spring-hat-a8qw0bvp-pooler.eastus2.azure.neon.tech/neondb?sslmode=require';

async function benchmark() {
    console.log('🚀 Real-World Performance Benchmark\n');
    
    const conn = new Connection(connStr, 10);
    
    // Warmup - create actual connection
    console.log('🔥 Warming up (creating connection)...');
    const warmStart = Date.now();
    await conn.query('SELECT 1');
    console.log(`   First query: ${Date.now() - warmStart}ms (includes connection creation)\n`);
    
    // Now measure real performance
    console.log('═══════════════════════════════════════');
    console.log('⚡ REAL PERFORMANCE (Warm Connection)');
    console.log('═══════════════════════════════════════\n');
    
    // Test 1: Simple query (10 runs)
    console.log('📊 Test 1: Simple SELECT (10 runs)');
    const times1 = [];
    for (let i = 0; i < 10; i++) {
        const start = Date.now();
        await conn.query('SELECT 1');
        times1.push(Date.now() - start);
    }
    console.log(`   Min: ${Math.min(...times1)}ms`);
    console.log(`   Max: ${Math.max(...times1)}ms`);
    console.log(`   Avg: ${Math.round(times1.reduce((a,b) => a+b) / times1.length)}ms\n`);
    
    // Test 2: Query with data (10 runs)
    console.log('📊 Test 2: SELECT 10 users (10 runs)');
    const times2 = [];
    for (let i = 0; i < 10; i++) {
        const start = Date.now();
        await conn.query('SELECT * FROM users LIMIT 10');
        times2.push(Date.now() - start);
    }
    console.log(`   Min: ${Math.min(...times2)}ms`);
    console.log(`   Max: ${Math.max(...times2)}ms`);
    console.log(`   Avg: ${Math.round(times2.reduce((a,b) => a+b) / times2.length)}ms\n`);
    
    // Test 3: Parameterized query (10 runs)
    console.log('📊 Test 3: Parameterized query (10 runs)');
    const times3 = [];
    for (let i = 0; i < 10; i++) {
        const start = Date.now();
        await conn.query('SELECT * FROM users WHERE id = $1', [2001 + i]);
        times3.push(Date.now() - start);
    }
    console.log(`   Min: ${Math.min(...times3)}ms`);
    console.log(`   Max: ${Math.max(...times3)}ms`);
    console.log(`   Avg: ${Math.round(times3.reduce((a,b) => a+b) / times3.length)}ms\n`);
    
    // Test 4: Prepared statement (10 runs)
    console.log('📊 Test 4: Prepared statement (10 runs)');
    conn.prepare('getUser', 'SELECT * FROM users WHERE id = $1');
    const times4 = [];
    for (let i = 0; i < 10; i++) {
        const start = Date.now();
        await conn.execute('getUser', [2001 + i]);
        times4.push(Date.now() - start);
    }
    console.log(`   Min: ${Math.min(...times4)}ms`);
    console.log(`   Max: ${Math.max(...times4)}ms`);
    console.log(`   Avg: ${Math.round(times4.reduce((a,b) => a+b) / times4.length)}ms\n`);
    
    // Test 5: Concurrent queries
    console.log('📊 Test 5: 10 concurrent queries');
    const start5 = Date.now();
    await Promise.all(
        Array.from({ length: 10 }, (_, i) => 
            conn.query('SELECT * FROM users WHERE id = $1', [2001 + i])
        )
    );
    const time5 = Date.now() - start5;
    console.log(`   Total: ${time5}ms`);
    console.log(`   Per query: ${Math.round(time5 / 10)}ms (parallel)\n`);
    
    // Test 6: Pipeline
    console.log('📊 Test 6: Pipeline 10 queries');
    const start6 = Date.now();
    await conn.pipeline(Array.from({ length: 10 }, () => 'SELECT 1'));
    const time6 = Date.now() - start6;
    console.log(`   Total: ${time6}ms`);
    console.log(`   Per query: ${Math.round(time6 / 10)}ms (batched)\n`);
    
    // Test 7: Large result set
    console.log('📊 Test 7: SELECT 100 users (5 runs)');
    const times7 = [];
    for (let i = 0; i < 5; i++) {
        const start = Date.now();
        await conn.query('SELECT * FROM users LIMIT 100');
        times7.push(Date.now() - start);
    }
    console.log(`   Min: ${Math.min(...times7)}ms`);
    console.log(`   Max: ${Math.max(...times7)}ms`);
    console.log(`   Avg: ${Math.round(times7.reduce((a,b) => a+b) / times7.length)}ms\n`);
    
    // Summary
    console.log('═══════════════════════════════════════');
    console.log('📊 SUMMARY (Average Times)');
    console.log('═══════════════════════════════════════');
    console.log(`Simple SELECT:          ${Math.round(times1.reduce((a,b) => a+b) / times1.length)}ms`);
    console.log(`SELECT 10 rows:         ${Math.round(times2.reduce((a,b) => a+b) / times2.length)}ms`);
    console.log(`Parameterized query:    ${Math.round(times3.reduce((a,b) => a+b) / times3.length)}ms`);
    console.log(`Prepared statement:     ${Math.round(times4.reduce((a,b) => a+b) / times4.length)}ms`);
    console.log(`Concurrent (10):        ${Math.round(time5 / 10)}ms per query`);
    console.log(`Pipeline (10):          ${Math.round(time6 / 10)}ms per query`);
    console.log(`SELECT 100 rows:        ${Math.round(times7.reduce((a,b) => a+b) / times7.length)}ms`);
    console.log('═══════════════════════════════════════');
    
    conn.close();
    console.log('\n✅ Benchmark complete');
}

benchmark().catch(console.error);
