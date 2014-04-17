///////////////////////////////////////////////////////////////////////////////
/// \author (c) Marco Paland (marco@paland.com)
///             2014, PALANDesign Hannover, Germany
///
/// \license LGPLv3
/// sqlitepp is free software: you can redistribute it and/or modify
/// it under the terms of the GNU Lesser Public License as published by
/// the Free Software Foundation, either version 3 of the License, or
/// (at your option) any later version.
/// sqlitepp is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
/// GNU Lesser Public License for more details.
/// You should have received a copy of the GNU Lesser Public License
/// along with decom. If not, see <http://www.gnu.org/licenses/>.
///
///
/// \brief sqlitepp classes
/// sqlitepp is a single header C++ wrapper for SQLite3
///
/// \changelog
/// 1.6.0   Removed str() function, it's implicit now via operator
///
/// 1.5.0   Added 'invalid' type (-1) for field_type
///         row [""] operator can return invalid ref as invalid field_type now
///
/// 1.4.1   Fixed support for binding empty vectors and strings
///
/// 1.4.0   Added query operator<< for TEXT
///
/// 1.3.0   Added 'vacuum' function for db defragmentation
///         Minor bugfixes
///
/// 1.2.1   Minor cleanups and fixes
///
/// 1.2.0   Added 'str' function on field_type to stringify fields
///         Added 'use_abort' function
///
/// 1.1.0   First inital release
///         Added some enhancements for field_type access
///
/// 1.0.0   All ideas coded, first tests okay
///
///////////////////////////////////////////////////////////////////////////////

#ifndef _SQLITEPP_H_
#define _SQLITEPP_H_

#define SQLITEPP_VERSION        "1.6.0"
#define SQLITEPP_VERSION_NUMBER 1006000   // (X*1000000 + Y*1000 + Z)

#include "sqlite3/sqlite3.h"

#include <vector>
#include <string>
#include <sstream>
#include <cstdint>


namespace sqlitepp {

/*
 * Representation of a single result field
 */
struct field_type
{
  // ctors
  field_type()
    : type_(-1) { }
  field_type(std::string const& name)
    : name_(name), type_(SQLITE_NULL) { int_ = 0; }
  field_type(int i, std::string const& name)
    : name_(name), type_(SQLITE_INTEGER) { int_ = i; }
  field_type(::sqlite3_int64 i64, std::string const& name)
    : name_(name), type_(SQLITE_INTEGER) { int_ = i64; }
  field_type(double d, std::string const& name)
    : name_(name), type_(SQLITE_FLOAT)  { float_ = d; }
  field_type(std::string s, std::string const& name)
    : name_(name), type_(SQLITE_TEXT) { str_ = s; }
  field_type(std::vector<std::uint8_t> v, std::string const& name)
    : name_(name), type_(SQLITE_BLOB) { vec_ = v; }

  // access field value
  operator int() const { return static_cast<int>(int_); }
  operator ::sqlite_int64() const { return int_; }
  operator double() const { return float_; }
  operator std::string() const {
    switch (type_) {
      case SQLITE_TEXT    : return str_;
      case SQLITE_INTEGER : { std::stringstream s; s << int_; return s.str(); }
      case SQLITE_FLOAT   : { std::stringstream s; s << float_; return s.str(); }
      case SQLITE_BLOB    : return "BLOB";
      case SQLITE_NULL    : return "NULL";
      case -1             : return "invalid";
      default             : return "";
    }
  }
  operator std::vector<std::uint8_t>() const { return vec_; }

  // get field (column) name
  inline std::string name() const { return name_; }

  // get field (column) type
  inline int type() const { return type_; }

  // returns true if field is NULL
  inline bool is_null() const { return type_ == SQLITE_NULL; }

private:
  std::int64_t              int_;   // int data
  double                    float_; // float data
  std::vector<std::uint8_t> vec_;   // vector (blob) data
  std::string               str_;   // string (text) data
  std::string               name_;  // field (col) name
  int                       type_;  // sqlte type
};


/*
 * Representation of a result row
 */
class row : public std::vector<field_type>
{
public:
  // access field by index
  const_reference operator[](size_type idx) const {
    return at(idx);
  }

  // access field by name
  const_reference operator[](const char* colname) const {
    const static field_type invalid_ref;
    for (row::const_iterator it = begin(); it != end(); ++it) {
      if (it->name() == colname) {
        return *it;
      }
    }
    return invalid_ref;  // no match - invalid answer
  }

  // return the number of fields in this row
  size_type num_fields() const { return size(); }
};


/*
 * Representation of a complete result set (all columns and rows)
 */
class result : public std::vector<row>
{
public:
  // default ctor
  result() { };

  // destroy result set
  ~result() { }

  // return the number of rows in this result set
  size_type num_rows() const { return size(); }
};


/*
 * Database class
 */
class db
{
public:
  db(std::string name, bool initial_open = true, int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)
    : name_(name)
    , open_(false)
    , db_(nullptr)
  {
    if (initial_open) {
      // inital connect
      (void)open(flags);
    }
  }

  ~db()
  {
    // close the db
    (void)close();
  }

  // open (connect) the database
  int open(int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)
  {
    int err = ::sqlite3_open_v2(name_.c_str(), &db_, flags, nullptr);
    open_ = !err;
    return err;
  }

  // close the database
  int close()
  {
    int err = ::sqlite3_close_v2(db_);
    open_ = false;
    db_   = nullptr;
    return err;
  }

  // returns true if the database is open
  inline bool is_open() const { return open_; }

  // SQLite3 access
  inline ::sqlite3* get() const { return db_; }
  inline ::sqlite3* operator()() const { return db_; }

  // SQLite version
  inline std::string version() { return std::string(SQLITE_VERSION); }

  // database defragmentation
  int vacuum()
  {
    return ::sqlite3_exec(db_, "VACUUM;", nullptr, nullptr, nullptr);
  }

private:
  db& operator=(const db&) { }    // no assignment

private:
  ::sqlite3*        db_;    // associated db
  const std::string name_;  // db filename
  bool              open_;  // db open status
};


/*
 * Representation of a query
 */
class query
{
public:
  query(const db& db, const std::string& query = "")
    : db_(db)
    , query_(query) { }

  ~query() { }

  // << operator for query assembly
  // CAUTION: Be aware of SQL injection when directly passing user input in the query. Better use explicit binding.
  template <class T>
  inline query& operator<< (const T& value)
  {
    query_ << value;
    return *this;
  }

  // specialized << operator to bind blobs
  inline query& operator<< (std::vector<std::uint8_t> const& blob)
  {
    bind_type b(bind_.size() + 1U, &blob);
    bind_.push_back(b);
    return *this;
  }

  // specialized << operator to bind TEXT
  inline query& operator<< (std::string const& text)
  {
    bind_type b(bind_.size() + 1U, &text);
    bind_.push_back(b);
    return *this;
  }

  // use "exec()" if you don't expect a result set, like a DELETE or UPDATE statement
  int exec(const char* query = "") {
    int err = ::sqlite3_prepare_v2(db_(), *query ? query : query_.str().c_str(), -1, &stmt_, nullptr);
    query_.str("");
    if (err != SQLITE_OK)   { (void)::sqlite3_finalize(stmt_); return err; }
    err = do_bind();
    if (err != SQLITE_OK)   { (void)::sqlite3_finalize(stmt_); return err; }
    err = ::sqlite3_step(stmt_);
    if (err != SQLITE_DONE) { (void)::sqlite3_finalize(stmt_); return err; }
    return ::sqlite3_finalize(stmt_);
  }

  // use "use()" to get a single row. You MUST call use_next() or use_abort() after!
  row use(const char* query = "") {
    int err = ::sqlite3_prepare_v2(db_(), *query ? query : query_.str().c_str(), -1, &stmt_, nullptr);
    query_.str("");
    if (err != SQLITE_OK) { (void)::sqlite3_finalize(stmt_); return row(); }
    err = do_bind();
    if (err != SQLITE_OK) { (void)::sqlite3_finalize(stmt_); return row(); }
    return use_next();
  }

  // use "use_next()" to get the next row after use()/use_next()
  // call use_next() until it returns an empty row or call use_abort() to finish prematurely
  row use_next() {
    row _row;
    if (::sqlite3_step(stmt_) == SQLITE_ROW) {
      for (int i = 0; i < sqlite3_column_count(stmt_); ++i) {
        switch (::sqlite3_column_type(stmt_, i))
        {
          case SQLITE_INTEGER:
            _row.push_back(field_type(sqlite3_column_int(stmt_, i), ::sqlite3_column_name(stmt_, i)));
            break;
          case SQLITE_FLOAT:
            _row.push_back(field_type(sqlite3_column_double(stmt_, i), ::sqlite3_column_name(stmt_, i)));
            break;
          case SQLITE_BLOB:
            {
              const std::uint8_t* blob = reinterpret_cast<const std::uint8_t*>(::sqlite3_column_blob(stmt_, i));
              std::vector<std::uint8_t> v(&blob[0], &blob[::sqlite3_column_bytes(stmt_, i)]);
              _row.push_back(field_type(v, ::sqlite3_column_name(stmt_, i)));
            }
            break;
          case SQLITE_TEXT:
            {
              std::string text(reinterpret_cast<const char*>(::sqlite3_column_text(stmt_, i)));
              _row.push_back(field_type(text, ::sqlite3_column_name(stmt_, i)));
            }
            break;
          case SQLITE_NULL:
            _row.push_back(field_type(::sqlite3_column_name(stmt_, i)));
            break;
          default:
            _row.push_back(field_type(0, ::sqlite3_column_name(stmt_, i)));
          break;
        }
      }
    }
    else {
      // finalize statement
      (void)::sqlite3_finalize(stmt_);
    }
    return _row;
  }

  // call this if use()/use_next() should be aborted before use_next() finished (returned an empty row)
  // this is a mandatory call!
  int use_abort() {
    return::sqlite3_finalize(stmt_);
  }

  // use "store()" if you want to get the complete result in memory
  result store(const char* query = "") {
    result _res;
    row _row = use(query);
    while (!_row.empty()) {
      _res.push_back(_row);
      _row = use_next();
    }
    return _res;
  }

  // return the last inserted rowid
  inline std::int64_t insert_id() const { return ::sqlite3_last_insert_rowid(db_()); }

  // return count of affected rows of the last statement (like in DELETE/UPDATE)
  inline std::int64_t affected_rows() const { return ::sqlite3_changes(db_()); }

  // bind a BLOB or TEXT to query
  // CAUTION: vector and string MUST BE constant until end of query execution of exec()/use()/store()
  void bind(int param, std::vector<std::uint8_t> const& blob) {
    bind_type b(param, &blob);
    bind_.push_back(b);
  }
  void bind(int param, std::string const& text) {
    bind_type t(param, &text);
    bind_.push_back(t);
  }
  void bind(const char* param, std::vector<std::uint8_t> const& blob) {
    bind_type b(param, &blob);
    bind_.push_back(b);
  }
  void bind(const char* param, std::string const& text) {
    bind_type t(param, &text);
    bind_.push_back(t);
  }

private:
  int do_bind() {
    int err = SQLITE_OK;
    for (std::vector<bind_type>::const_iterator it = bind_.begin(); it != bind_.end(); ++it) {
      if (it->type_== SQLITE_BLOB) {
        const std::vector<std::uint8_t>* v = static_cast<const std::vector<std::uint8_t>*>(it->ptr_);
        err = ::sqlite3_bind_blob(stmt_, !it->param_str_ ? it->param_num_ : ::sqlite3_bind_parameter_index(stmt_, it->param_str_), v->size() ? (const void*)&(*v)[0] : nullptr, (int)v->size(), SQLITE_STATIC);
        if (err != SQLITE_OK) break;
      }
      if (it->type_ == SQLITE_TEXT) {
        const std::string* s = static_cast<const std::string*>(it->ptr_);
        err = ::sqlite3_bind_text(stmt_, !it->param_str_ ? it->param_num_ : ::sqlite3_bind_parameter_index(stmt_, it->param_str_), s->size() ? s->c_str() : nullptr, (int)s->size(), SQLITE_STATIC);
        if (err != SQLITE_OK) break;
      }
      if (it->type_ == SQLITE_NULL) {
        err = ::sqlite3_bind_null(stmt_, !it->param_str_ ? it->param_num_ : ::sqlite3_bind_parameter_index(stmt_, it->param_str_));
        if (err != SQLITE_OK) break;
      }
    }
    bind_.clear();  // clear bindings
    return err;
  }

private:
  const db&         db_;    // associated db
  ::sqlite3_stmt*   stmt_;  // statement
  std::stringstream query_; // query

  typedef struct struct_bind_type {
    const void* ptr_;
    const char* param_str_;
    int         param_num_;
    int         type_;
    struct_bind_type(int param, const std::vector<std::uint8_t>* blob) : param_num_(param), param_str_(nullptr), ptr_(blob), type_(SQLITE_BLOB) { };
    struct_bind_type(int param, const std::string* text) : param_num_(param), param_str_(nullptr), ptr_(text), type_(SQLITE_TEXT) { };
    struct_bind_type(const char* param, const std::vector<std::uint8_t>* blob) : param_num_(0), param_str_(param), ptr_(blob), type_(SQLITE_BLOB) { };
    struct_bind_type(const char* param, const std::string* text) : param_num_(0), param_str_(param), ptr_(text), type_(SQLITE_TEXT) { };
  } bind_type;
  std::vector<bind_type> bind_;
};


/*
 * Representation of a transaction
 */
class transaction
{
public:
  // isolation levels - see SQLite3 documentation
  enum level_type {
    deferred,
    immediate,
    exclusve
  };

  transaction(const db& db, level_type level = deferred)
    : db_(db)
    , finished_(true)   // don't bother rolling it back if ctor fails
  {
    (void)begin(level);
  }

  ~transaction() {
    if (!finished_) { rollback(); }
  }

  // start transaction
  int begin(level_type level = deferred) {
    int err = ::sqlite3_exec(db_(), level == deferred ? "BEGIN TRANSACTION;" : (level == immediate ? "BEGIN IMMEDIATE TRANSACTION;" : "BEGIN EXCLUSIVE TRANSACTION;"), nullptr, nullptr, nullptr);
    finished_ = (err != SQLITE_OK);
    return err;
  }

  // commit transaction
  int commit() {
    int err = ::sqlite3_exec(db_(), "COMMIT;", nullptr, nullptr, nullptr);
    finished_ = true;
    return err;
  }

  // rollback transaction
  int rollback() {
    int err = ::sqlite3_exec(db_(), "ROLLBACK;", nullptr, nullptr, nullptr);
    finished_ = true;
    return err;
  }

private:
  transaction& operator=(const transaction&) { }    // no assignment

private:
  const db& db_;
  bool finished_;
};

} // namespace sqlitepp

#endif  // _SQLITEPP_H_
