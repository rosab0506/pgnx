export interface ConnectionConfig {
  connectionString: string;
  poolSize?: number;
}

export interface QueryResult<T = any> {
  rows: T[];
  rowCount: number;
}

export class Connection {
  constructor(connectionString: string, poolSize?: number);
  
  query<T = any>(sql: string, params?: any[]): Promise<T[]>;
  prepare(name: string, sql: string): void;
  execute<T = any>(name: string, params?: any[]): Promise<T[]>;
  pipeline(queries: string[]): Promise<number[]>;
  listen(channel: string, callback: (payload: string) => void): void;
  unlisten(channel: string): void;
  close(): void;
}

export interface PoolConfig {
  min?: number;
  max?: number;
  idleTimeoutMillis?: number;
  connectionTimeoutMillis?: number;
}

export class Pool {
  constructor(config: string | ConnectionConfig);
  
  query<T = any>(sql: string, params?: any[]): Promise<T[]>;
  connect(): Promise<PoolClient>;
  end(): Promise<void>;
}

export interface PoolClient {
  query<T = any>(sql: string, params?: any[]): Promise<T[]>;
  release(): void;
}
