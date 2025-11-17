const { Client } = require('pg');
const { Connection } = require('./index.js');

const connStr = process.env.DATABASE_URL || 'postgresql://myuser:mypassword@localhost:5432/mydb';

async function benchmarkPg() {
  const client = new Client(connStr);
  
  const t0 = Date.now();
  await client.connect();
  const connTime = Date.now() - t0;
  
  const t1 = Date.now();
  await client.query('SELECT 1');
  const queryTime = Date.now() - t1;
  
  const t2 = Date.now();
  await client.query('BEGIN');
  await client.query('SELECT 1');
  await client.query('SELECT 2');
  await client.query('COMMIT');
  const pipelineTime = Date.now() - t2;
  
  await client.end();
  return { connTime, queryTime, pipelineTime };
}

async function benchmarkPgnx() {
  const t0 = Date.now();
  const conn = new Connection(connStr, 1);
  await conn.query('SELECT 1');
  const connTime = Date.now() - t0;
  
  const t1 = Date.now();
  await conn.query('SELECT 1');
  const queryTime = Date.now() - t1;
  
  const t2 = Date.now();
  await conn.pipeline(['SELECT 1', 'SELECT 2', 'SELECT 3']);
  const pipelineTime = Date.now() - t2;
  
  conn.close();
  return { connTime, queryTime, pipelineTime };
}

async function run() {
  console.log('Benchmarking pg vs pgnx...\n');
  
  const pg = await benchmarkPg();
  const pgnx = await benchmarkPgnx();
  
  console.log('Operation\t\tpg\tpgnx\tImprovement');
  console.log('─'.repeat(60));
  console.log(`Connection\t\t${pg.connTime}ms\t${pgnx.connTime}ms\t${(pg.connTime/pgnx.connTime).toFixed(1)}x`);
  console.log(`Simple Query\t\t${pg.queryTime}ms\t${pgnx.queryTime}ms\t${(pg.queryTime/pgnx.queryTime).toFixed(1)}x`);
  console.log(`Pipeline (3 queries)\t${pg.pipelineTime}ms\t${pgnx.pipelineTime}ms\t${(pg.pipelineTime/pgnx.pipelineTime).toFixed(1)}x`);
}

run().catch(console.error);
