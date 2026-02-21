export interface PoolStatus {
    available: number;
    current: number;
    max: number;
    closed: boolean;
}

export class Connection {
    constructor(connectionString: string, poolSize?: number);

    /** Execute a query asynchronously with optional parameters */
    query<T = any>(sql: string, params?: any[]): Promise<T[]>;

    /** Execute a query synchronously with optional parameters */
    querySync<T = any>(sql: string, params?: any[]): T[];

    /** Register a prepared statement */
    prepare(name: string, sql: string): void;

    /** Execute a previously prepared statement */
    execute<T = any>(name: string, params?: any[]): Promise<T[]>;

    /** Execute multiple queries in a pipeline, returns affected row counts */
    pipeline(queries: string[]): Promise<number[]>;

    /** Begin a transaction */
    begin(): Promise<void>;

    /** Commit the current transaction */
    commit(): Promise<void>;

    /** Rollback the current transaction */
    rollback(): Promise<void>;

    /** Listen for PostgreSQL NOTIFY events on a channel */
    listen(channel: string, callback: (payload: string) => void): void;

    /** Stop listening on a channel */
    unlisten(channel: string): void;

    /** Get current pool status */
    poolStatus(): PoolStatus;

    /** Close all connections and clean up */
    close(): void;
}
