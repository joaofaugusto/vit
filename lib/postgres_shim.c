// postgres_shim.c — libpq wrappers for Vit
//
// PGconn* and PGresult* are cast to char* so Vit's str can hold them as
// opaque handles — same pattern as sqlite_shim.c.
//
// Parameterized queries use a global param buffer (up to 16 params).
// Call vit_postgres_param() for each value, then vit_postgres_query_p().
//
// Requires: libpq-dev
//   sudo apt install libpq-dev

#include <libpq-fe.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ── connection ────────────────────────────────────────────────────────────────

// conninfo: "postgresql://user:pass@host:5432/dbname"
//        or "host=localhost port=5432 dbname=mydb user=foo password=bar"
char* vit_postgres_connect(const char* conninfo) {
    PGconn* conn = PQconnectdb(conninfo);
    return (char*)conn;
}

// Returns 1 if connected, 0 otherwise.
int vit_postgres_ok(char* conn) {
    return PQstatus((PGconn*)conn) == CONNECTION_OK ? 1 : 0;
}

void vit_postgres_close(char* conn) {
    PQfinish((PGconn*)conn);
}

const char* vit_postgres_errmsg(char* conn) {
    const char* msg = PQerrorMessage((PGconn*)conn);
    return msg ? msg : "";
}

// ── simple exec (DDL, INSERT, UPDATE, DELETE — no rows expected) ───────────────

// Returns 1 on success (PGRES_COMMAND_OK or PGRES_TUPLES_OK), 0 on error.
int vit_postgres_exec(char* conn, const char* sql) {
    PGresult* res = PQexec((PGconn*)conn, sql);
    ExecStatusType status = PQresultStatus(res);
    PQclear(res);
    return (status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK) ? 1 : 0;
}

// ── query returning rows ──────────────────────────────────────────────────────

char* vit_postgres_query(char* conn, const char* sql) {
    PGresult* res = PQexec((PGconn*)conn, sql);
    return (char*)res;
}

// Returns 1 if the result is OK (has rows or command succeeded).
int vit_postgres_result_ok(char* res) {
    ExecStatusType s = PQresultStatus((PGresult*)res);
    return (s == PGRES_COMMAND_OK || s == PGRES_TUPLES_OK) ? 1 : 0;
}

const char* vit_postgres_result_errmsg(char* res) {
    const char* msg = PQresultErrorMessage((PGresult*)res);
    return msg ? msg : "";
}

int vit_postgres_nrows(char* res) {
    return PQntuples((PGresult*)res);
}

int vit_postgres_ncols(char* res) {
    return PQnfields((PGresult*)res);
}

// Returns the string value at (row, col). Row and col are 0-based.
const char* vit_postgres_col(char* res, int row, int col) {
    if (PQgetisnull((PGresult*)res, row, col)) return "";
    const char* val = PQgetvalue((PGresult*)res, row, col);
    return val ? val : "";
}

// Returns the integer value at (row, col).
int vit_postgres_col_int(char* res, int row, int col) {
    if (PQgetisnull((PGresult*)res, row, col)) return 0;
    const char* val = PQgetvalue((PGresult*)res, row, col);
    return val ? atoi(val) : 0;
}

// Returns 1 if the value at (row, col) is NULL.
int vit_postgres_is_null(char* res, int row, int col) {
    return PQgetisnull((PGresult*)res, row, col) ? 1 : 0;
}

// Returns the column name at index col (0-based).
const char* vit_postgres_col_name(char* res, int col) {
    const char* name = PQfname((PGresult*)res, col);
    return name ? name : "";
}

void vit_postgres_free(char* res) {
    PQclear((PGresult*)res);
}

// ── parameterized queries ($1, $2, ...) ───────────────────────────────────────

#define MAX_PARAMS 16
static const char* pg_params[MAX_PARAMS];
static int         pg_param_count = 0;

// Add a parameter value. Call before vit_postgres_query_p().
// Parameters are bound in order: first call → $1, second → $2, etc.
void vit_postgres_param(const char* val) {
    if (pg_param_count < MAX_PARAMS)
        pg_params[pg_param_count++] = val;
}

// Executes a parameterized query using the params set by vit_postgres_param().
// n must match the number of vit_postgres_param() calls made.
// Resets the param buffer after execution.
char* vit_postgres_query_p(char* conn, const char* sql, int n) {
    PGresult* res = PQexecParams(
        (PGconn*)conn, sql,
        n, NULL,
        pg_params, NULL, NULL,
        0   // text results
    );
    pg_param_count = 0;
    return (char*)res;
}

// Convenience: exec with params (no rows expected — DDL/INSERT/UPDATE/DELETE).
// Returns 1 on success, 0 on error.
int vit_postgres_exec_p(char* conn, const char* sql, int n) {
    PGresult* res = PQexecParams(
        (PGconn*)conn, sql,
        n, NULL,
        pg_params, NULL, NULL,
        0
    );
    pg_param_count = 0;
    ExecStatusType status = PQresultStatus(res);
    PQclear(res);
    return (status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK) ? 1 : 0;
}
