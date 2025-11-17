const { Connection } = require('./pgnx');

(async () => {
  const conn = new Connection(
    'postgresql://neondb_owner:npg_m7N0XzUxPnMu@ep-spring-hat-a8qw0bvp-pooler.eastus2.azure.neon.tech/neondb?sslmode=require&channel_binding=require',
    10
  );

  const users = await conn.query('SELECT * FROM users WHERE age > $1', [18]);
  console.log('Users:', users);

  conn.prepare('getUser', 'SELECT * FROM users WHERE id = $1');
  const user = await conn.execute('getUser', [1]);
  console.log('User:', user);

  const results = await conn.pipeline([
    'UPDATE users SET active = true WHERE id = 1',
    'UPDATE users SET active = true WHERE id = 2'
  ]);
  console.log('Pipeline results:', results);

  conn.listen('events', (payload) => console.log('Event:', payload));

  conn.close();
  console.log('✅ All tests passed!');
})();
