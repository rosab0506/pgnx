const { Connection } = require('./index');

const connStr = 'postgresql://neondb_owner:npg_m7N0XzUxPnMu@ep-spring-hat-a8qw0bvp-pooler.eastus2.azure.neon.tech/neondb?sslmode=require&channel_binding=require'

async function test() {
    console.log('🔗 Connecting to Neon PostgreSQL...\n');
    const conn = new Connection(connStr);
    
    // First query triggers actual connection
    console.log('📊 First Query (creates actual connection)...');
    const startFirst = Date.now();
    await conn.query('SELECT 1');
    const firstTime = Date.now() - startFirst;
    
    console.log(`✅ First query completed: ${firstTime}ms (includes connection creation)\n`);
    
    try {
        // Test 1: Query users table
        console.log('📊 Test 1: Querying users table...');
        const startQuery = Date.now();
        const users = await conn.query('SELECT * FROM users LIMIT 10');
        const queryTime = Date.now() - startQuery;
        
        console.log(`✅ Query completed: ${queryTime}ms`);
        console.log(`📦 Rows retrieved: ${users.length}`);
        console.log('📋 Data:');
        console.log(JSON.stringify(users, null, 2));
        console.log();
        
        // Test 2: Count total users
        console.log('📊 Test 2: Counting total users...');
        const startCount = Date.now();
        const count = await conn.query('SELECT COUNT(*) as total FROM users');
        const countTime = Date.now() - startCount;
        
        console.log(`✅ Count completed: ${countTime}ms`);
        console.log(`👥 Total users: ${count[0].total}`);
        console.log();
        
        // Test 3: Prepared statement
        console.log('📊 Test 3: Testing prepared statement...');
        conn.prepare('getUser', 'SELECT * FROM users WHERE id = $1');
        const startPrep = Date.now();
        const user = await conn.execute('getUser', [1]);
        const prepTime = Date.now() - startPrep;
        
        console.log(`✅ Prepared statement executed: ${prepTime}ms`);
        console.log('📋 Result:', JSON.stringify(user, null, 2));
        console.log();
        
        // Test 4: Pipeline multiple queries
        console.log('📊 Test 4: Testing query pipeline...');
        const startPipe = Date.now();
        const results = await conn.pipeline([
            'SELECT 1 as test',
            'SELECT 2 as test',
            'SELECT 3 as test'
        ]);
        const pipeTime = Date.now() - startPipe;
        
        console.log(`✅ Pipeline completed: ${pipeTime}ms`);
        console.log('📋 Results:', results);
        console.log();
        
        // Summary
        console.log('═══════════════════════════════════════');
        console.log('📈 PERFORMANCE SUMMARY');
        console.log('═══════════════════════════════════════');
        console.log(`Connection Time:        ${firstTime}ms (actual)`);
        console.log(`Query Time (SELECT):    ${queryTime}ms`);
        console.log(`Count Time:             ${countTime}ms`);
        console.log(`Prepared Statement:     ${prepTime}ms`);
        console.log(`Pipeline (3 queries):   ${pipeTime}ms`);
        console.log('═══════════════════════════════════════');
        
    } catch (error) {
        console.error('❌ Error:', error.message);
        console.error('Stack:', error.stack);
    } finally {
        conn.close();
        console.log('\n✅ Connection closed');
    }
}

test().catch(console.error);
