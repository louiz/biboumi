#include <database/row.hpp>

template <typename... T>
class AsyncResult
{
  std::unique_ptr<Statement> statement{};
  std::string table_name;

public:
  AsyncResult(std::unique_ptr<Statement> s, const std::string& table_name):
    statement{std::move(s)},
    table_name{table_name}
  {}

  class iterator
  {
    using iterator_category = std::input_iterator_tag;
    using value_type = Row<T...>;
    using difference_type = std::ptrdiff_t;
    using pointer = Row<T...>*;
    using reference = Row<T...>&;

    Row<T...> row{};
    Statement* statement;
    bool is_end;

  public:
    iterator(Statement* s, const std::string& table_name, bool end=false):
      row{table_name},
      statement{s},
      is_end{end}
    {}

    reference operator*()
    {
      extract_row_values(this->row, *statement);
      return this->row;
    }

    bool operator==(const iterator& o) const
    {
      if (this->is_end && o.is_end)
        return true;
      return false;
    }
    bool operator!=(const iterator& o) const
    {
      return !(*this == o);
    }

    iterator& operator++()
    {
      if (statement->step() != StepResult::Row)
        this->is_end = true;
      return *this;
    }

    iterator& operator++(int)
    {
      iterator old = *this;
      if (statement->step() != StepResult::Row)
        this->is_end = true;
      return old;
    }
  };

  iterator begin() const
  {
    iterator it{this->statement.get(), this->table_name};
    ++it;
    return it;
  }
  iterator end() const
  {
    return {this->statement.get(), this->table_name, true};
  }
};
