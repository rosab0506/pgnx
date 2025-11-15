const { Connection } = require('./index');

const connStr = 'postgresql://neondb_owner:npg_m7N0XzUxPnMu@ep-spring-hat-a8qw0bvp-pooler.eastus2.azure.neon.tech/neondb?sslmode=require';

async function benchmark() {
    console.log('🏁 BENCHMARK: Connection Pool Performance\n');
    
    const conn = new Connection(connStr, 10);
    
    // Warmup
    console.log('🔥 Warming up...');
    await conn.query('SELECT 1');
    console.log('✅ Ready\n');
    
    // Benchmark 1: Sequential queries
    console.log('📊 Benchmark 1: 10 Sequential Queries');
    const seqStart = Date.now();
    for (let i = 0; i < 10; i++) {
        await conn.query('SELECT * FROM users WHERE id = $1', [i + 1]);
    }
    const seqTime = Date.now() - seqStart;
    console.log(`   Total: ${seqTime}ms`);
    console.log(`   Average: ${Math.round(seqTime / 10)}ms per query\n`);
    
    // Benchmark 2: Parallel queries (connection pool)
    console.log('📊 Benchmark 2: 10 Parallel Queries (Pool)');
    const parStart = Date.now();
    await Promise.all(
        Array.from({ length: 10 }, (_, i) => 
            conn.query('SELECT * FROM users WHERE id = $1', [i + 1])
        )
    );
    const parTime = Date.now() - parStart;
    console.log(`   Total: ${parTime}ms`);
    console.log(`   Speedup: ${(seqTime / parTime).toFixed(2)}x faster\n`);
    
    // Benchmark 3: Pipeline
    console.log('📊 Benchmark 3: 10 Queries via Pipeline');
    const pipeStart = Date.now();
    await conn.pipeline(
        Array.from({ length: 10 }, () => 'SELECT 1')
    );
    const pipeTime = Date.now() - pipeStart;
    console.log(`   Total: ${pipeTime}ms\n`);
    
    // Benchmark 4: Prepared statements
    console.log('📊 Benchmark 4: 10 Prepared Statement Executions');
    conn.prepare('bench', 'SELECT * FROM users WHERE id = $1');
    const prepStart = Date.now();
    for (let i = 0; i < 10; i++) {
        await conn.execute('bench', [i + 1]);
    }
    const prepTime = Date.now() - prepStart;
    console.log(`   Total: ${prepTime}ms`);
    console.log(`   Average: ${Math.round(prepTime / 10)}ms per execution\n`);
    
    // Summary
    console.log('═══════════════════════════════════════');
    console.log('🏆 RESULTS (10 queries each)');
    console.log('═══════════════════════════════════════');
    console.log(`Sequential:     ${seqTime}ms`);
    console.log(`Parallel:       ${parTime}ms  (${(seqTime / parTime).toFixed(2)}x faster)`);
    console.log(`Pipeline:       ${pipeTime}ms`);
    console.log(`Prepared:       ${prepTime}ms`);
    console.log('═══════════════════════════════════════');
    
    conn.close();
}

benchmark().catch(console.error);
