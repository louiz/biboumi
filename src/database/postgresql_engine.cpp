#include <biboumi.h>
#ifdef PQ_FOUND

#include <utils/scopeguard.hpp>

#include <database/query.hpp>

#include <database/postgresql_engine.hpp>

#include <database/postgresql_statement.hpp>

#include <logger/logger.hpp>

PostgresqlEngine::PostgresqlEngine(PGconn*const conn):
    conn(conn)
{}

PostgresqlEngine::~PostgresqlEngine()
{
  PQfinish(this->conn);
}

std::unique_ptr<DatabaseEngine> PostgresqlEngine::open(const std::string& conninfo)
{
  PGconn* con = PQconnectdb(conninfo.data());

  if (!con)
    {
      log_error("Failed to allocate a Postgresql connection");
      throw std::runtime_error("");
    }
  const auto status = PQstatus(con);
  if (status != CONNECTION_OK)
    {
      const char* errmsg = PQerrorMessage(con);
      log_error("Postgresql connection failed: ", errmsg);
      throw std::runtime_error("failed to open connection.");
    }
  return std::make_unique<PostgresqlEngine>(con);
}

std::set<std::string> PostgresqlEngine::get_all_columns_from_table(const std::string& table_name)
{
  const auto query = "SELECT column_name from information_schema.columns where table_name='" + table_name + "'";
  auto statement = this->prepare(query);
  std::set<std::string> columns;

  while (statement->step() == StepResult::Row)
    columns.insert(statement->get_column_text(0));

  return columns;
}

std::tuple<bool, std::string> PostgresqlEngine::raw_exec(const std::string& query)
{
#ifdef DEBUG_SQL_QUERIES
  log_debug("SQL QUERY: ", query);
  const auto timer = make_sql_timer();
#endif
  PGresult* res = PQexec(this->conn, query.data());
  auto sg = utils::make_scope_guard([res](){
      PQclear(res);
  });

  auto res_status = PQresultStatus(res);
  if (res_status != PGRES_COMMAND_OK)
    return std::make_tuple(false, PQresultErrorMessage(res));
  return std::make_tuple(true, std::string{});
}

std::unique_ptr<Statement> PostgresqlEngine::prepare(const std::string& query)
{
  return std::make_unique<PostgresqlStatement>(query, this->conn);
}

void PostgresqlEngine::extract_last_insert_rowid(Statement& statement)
{
  this->last_inserted_rowid = statement.get_column_int64(0);
}

std::string PostgresqlEngine::get_returning_id_sql_string(const std::string& col_name)
{
  return " RETURNING " + col_name;
}

std::string PostgresqlEngine::id_column_type()
{
  return "SERIAL";
}

#endif
