const { Connection } = require('../index');

const connStr = process.env.DATABASE_URL || 'postgresql://postgres:postgres@localhost:5432/testdb';

async function runTests() {
    console.log('🧪 Running PGNX Tests...\n');
    
    const conn = new Connection(connStr, 5);
    
    try {
        // Test 1: Basic connection
        console.log('Test 1: Basic connection...'); 
        const result = await conn.query('SELECT 1 as num');
        console.assert(result[0].num === 1, 'Basic query failed');
        console.log('✅ Pass\n');
        
        // Test 2: Create test table
        console.log('Test 2: Create table...');
        await conn.query('DROP TABLE IF EXISTS test_users');
        await conn.query('CREATE TABLE test_users (id SERIAL PRIMARY KEY, name TEXT, age INT)');
        console.log('✅ Pass\n');
        
        // Test 3: Insert data
        console.log('Test 3: Insert with parameters...');
        await conn.query('INSERT INTO test_users (name, age) VALUES ($1, $2)', ['Alice', 25]);
        await conn.query('INSERT INTO test_users (name, age) VALUES ($1, $2)', ['Bob', 30]);
        console.log('✅ Pass\n');
        
        // Test 4: Query with parameters
        console.log('Test 4: Query with parameters...');
        const users = await conn.query('SELECT * FROM test_users WHERE age > $1', [20]);
        console.assert(users.length === 2, 'Query with params failed');
        console.log('✅ Pass\n');
        
        // Test 5: Prepared statements
        console.log('Test 5: Prepared statements...');
        conn.prepare('getUser', 'SELECT * FROM test_users WHERE name = $1');
        const user = await conn.execute('getUser', ['Alice']);
        console.assert(user[0].name === 'Alice', 'Prepared statement failed');
        console.log('✅ Pass\n');
        
        // Test 6: Pipeline
        console.log('Test 6: Pipeline queries...');
        const results = await conn.pipeline([
            'UPDATE test_users SET age = 26 WHERE name = \'Alice\'',
            'UPDATE test_users SET age = 31 WHERE name = \'Bob\''
        ]);
        console.assert(results[0] === 1 && results[1] === 1, 'Pipeline failed');
        console.log('✅ Pass\n');
        
        // Cleanup
        await conn.query('DROP TABLE test_users');
        
        console.log('🎉 All tests passed!\n');
        
    } catch (error) {
        console.error('❌ Test failed:', error.message);
        conn.close();
        process.exit(1);
    }
    
    // Wait a bit before closing to ensure all async operations complete
    setTimeout(() => {
        conn.close();
        process.exit(0);
    }, 100);
}

runTests();
