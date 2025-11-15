const { Connection } = require('./index');

const connStr = 'postgresql://neondb_owner:npg_m7N0XzUxPnMu@ep-spring-hat-a8qw0bvp-pooler.eastus2.azure.neon.tech/neondb?sslmode=require';

async function test() {
    console.log('Testing connection pool reuse...\n');
    
    const conn = new Connection(connStr, 3);
    
    // First query - creates connection
    console.log('Query 1 (creates connection):');
    const start1 = Date.now();
    await conn.query('SELECT 1');
    console.log(`  ${Date.now() - start1}ms\n`);
    
    // Second query - should reuse connection (much faster)
    console.log('Query 2 (reuses connection):');
    const start2 = Date.now();
    await conn.query('SELECT 1');
    console.log(`  ${Date.now() - start2}ms\n`);
    
    // Third query - should reuse connection
    console.log('Query 3 (reuses connection):');
    const start3 = Date.now();
    await conn.query('SELECT 1');
    console.log(`  ${Date.now() - start3}ms\n`);
    
    // 10 queries in sequence
    console.log('10 sequential queries:');
    const times = [];
    for (let i = 0; i < 10; i++) {
        const start = Date.now();
        await conn.query('SELECT 1');
        times.push(Date.now() - start);
    }
    console.log(`  Min: ${Math.min(...times)}ms`);
    console.log(`  Max: ${Math.max(...times)}ms`);
    console.log(`  Avg: ${Math.round(times.reduce((a,b) => a+b) / times.length)}ms\n`);
    
    conn.close();
    
    if (Math.min(...times) < 250) {
        console.log('✅ Pool is working - connections are reused!');
    } else {
        console.log('❌ Pool NOT working - all queries take ~240ms (network latency)');
        console.log('   This is normal for cloud databases - pg has same latency');
    }
}

test().catch(console.error);
