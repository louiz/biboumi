#include <biboumi.h>
#ifdef PQ_FOUND

#include <utils/scopeguard.hpp>
#include <utils/tolower.hpp>

#include <database/query.hpp>

#include <database/postgresql_engine.hpp>

#include <database/postgresql_statement.hpp>

#include <logger/logger.hpp>

#include <cstring>
#include <database/database.hpp>

PostgresqlEngine::PostgresqlEngine(PGconn*const conn):
    conn(conn)
{}

PostgresqlEngine::~PostgresqlEngine()
{
  PQfinish(this->conn);
}

static void logging_notice_processor(void*, const char* original)
{
  if (original && std::strlen(original) > 0)
    {
      std::string message{original, std::strlen(original) - 1};
      log_warning("PostgreSQL: ", message);
    }
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
      PQfinish(con);
      throw std::runtime_error("failed to open connection.");
    }
  PQsetNoticeProcessor(con, &logging_notice_processor, nullptr);
  return std::make_unique<PostgresqlEngine>(con);
}

std::map<std::string, std::string> PostgresqlEngine::get_all_columns_from_table(const std::string& table_name)
{
  const auto query = "SELECT column_name, data_type from information_schema.columns where table_name='" + table_name + "'";
  auto statement = this->prepare(query);
  std::map<std::string, std::string> columns;

  while (statement->step() == StepResult::Row)
    columns[utils::tolower(statement->get_column_text(0))] = utils::tolower(statement->get_column_text(1));

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

std::string PostgresqlEngine::id_column_type() const
{
  return "SERIAL";
}

std::string PostgresqlEngine::datetime_column_type() const
{
  return "TIMESTAMP";
}

void PostgresqlEngine::convert_date_format(DatabaseEngine& db)
{
  const auto table_name = Database::muc_log_lines.get_name();
  const std::string column_name = Database::Date::name;
  const std::string query = "ALTER TABLE " + table_name + " ALTER COLMUN " + column_name + " SET DATA TYPE timestamp USING to_timestamp(" + column_name + ")";

  auto result = db.raw_exec(query);
  if (!std::get<bool>(result))
    log_error("Failed to execute query: ", std::get<std::string>(result));
}

std::string PostgresqlEngine::escape_param_number(int nb) const
{
  return "to_timestamp(" + DatabaseEngine::escape_param_number(nb) + ")";
}

void PostgresqlEngine::init_session()
{
  const auto res = this->raw_exec("SET SESSION TIME ZONE 'UTC'");
  if (!std::get<bool>(res))
    log_error("Failed to set UTC timezone: ", std::get<std::string>(res));
}
long double PostgresqlEngine::epoch_to_floating_value(long double seconds) const
{
  return seconds;
}

#endif
