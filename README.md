# sqlitepp
sqlitepp is a C++ and STL orientated single header wrapper around the fantastic SQLite3 library.
It simplyfies the C API by using BLOBs as vectors and TEXT as strings and makes accessing result objects easy.
To provide a single versioned library the SQLite3 is included in the SQLite3 folder and updated when necessary.


## Classes

### field_type
This class represents a singe result field (column field of a row).

SQLite has 5 data types:
- INTEGER, stored in std::uint64_t
- FLOAT, stored in double
- TEXT, stored in std::string
- BLOB, stored in std::vector<std::uint8_t>
- NULL, internally handled, checked by is_null()

Use these sqlitepp types when accessing result fields.
There is an automatic type conversion/cast:
```c++
int i = row["num"];         // get data of num field (INTEGER) as integer
std::string s = row["num"]; // get data of num field (INTEGER) as string
double d = row["flo"];      // get data of flo field (FLOAT) as double
// CAUTION:
int f = row["flo"];         // ERROR: integer result flo field (FLOAT) is not defined/set!
```

Methods of field_type are:

name()
Returns the name of the field column.

type()
Returns the field type as sqlite3 type definition (SQLITE_INTEGER, SQLITE_TEXT etc.)

is_null()
Returns true if the field is NULL.


### row
This class represents a single result row.
Fields can be accessed via the [] operator by their numeric column index or by their column name.
Access by name is slower, because of the name lookup.

All std::vector operations are valid, because row is a std::vector of field_type

Methods of row are:

operator[](size_type idx)
Access the field by its index position in the row.

operator[](const char* colname)
Access the field by its column name.

num_fields()
Returns the field count of the row. (A wrapper for size())


### result
This class is a std::vector of row. It represents a complete result set.

Methods of result are:

num_rows()
Returns the number of rows of the result. (A wrapper for size())


### db
Main class that holds the sqlite3 object, which can be accessed via get() or, even shorter, via the () operator.

Methods of db are:

db(name, initial_open = true, flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)
Ctor which creates and optionally opens the database.

open(flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)
Opens the database with the apropriate access flags.

close()
Closes the db. The db is automatically closed when db goes out of scope.

version()
Returns the version of the SQLite3 database as string.

vacuum()
Executes the SQLite VACUUM command for database defragmentation.


### query
Class for query assembly and execution.
The << operator can add query parts or bind BLOB and TEXT data.

exec()
Executes the query and returns only the execution status. This can be used for commands like UPDATE or DELETE.

use()
Executes the query and returns the first row of the result set. This can be uses if only one row is needed or to get the result set row by row.
Repeatly call use_next() until use_next() returns an empty row or call use_abort() before.

use_next()
Returns the next row of the use() function.
An empty row is returned if no further rows exist.

use_abort()
Aborts use()/use_next() sequence before use_next() finished (returned an empty row).
This is a MANDATORY call if use()/use_next() need to be aborted.

store()
Executes the query and returns the complete result as result object.

bind()
Binds the given vector or string to the according params.
CAUTION: The vector or string must be constant until end of query execution!
The vector/string is internally not copied!


### transaction
The transaction class simplifies begin, commit and rollback of transactions.
Begin is called by class ctor, so an explicit begin isn't necessary.
Methods are:

begin() 
Begin the transaction with the according level flags. Default is deferred.

commit() 
Commit the transaction.

rollback() 
Rollback the transaction. Rollback is called by the dtor when transaction object goes out of scope.


## Usage
Here are some examples how to use sqlitepp:

Create and open the database by its filename
```c++
sqlitepp::db db(TEST_DB);
assert(db.is_open());
```
The following code asumes this db object.

Create a query and set the command in the ctor:
```c++
sqlitepp::query q(db, "DROP TABLE test;");
int err = q.exec();
```

Create a query and set the command in exec:
```c++
sqlitepp::query q(db, "THIS QUERY IS DISCARDED");
int err = q.exec("CREATE TABLE test (id INTEGER PRIMARY KEY NOT NULL, num INTEGER, name VARCHAR(20), flo FLOAT, data BLOB, comment TEXT);");
```

Insert a BLOB in the table and via explicit binding:
```c++
std::vector<std::uint8_t> blob(30U);
sqlitepp::query q(db);
q.bind(1, blob);
int err = q.exec("INSERT INTO test (data) VALUES (?1)");
int id  = q.insert_id();
```

Insert a TEXT using the alpha (@) index, build the query via the << operator:
```c++
std::string text("Test");
sqlitepp::query q(db);
q << "INSERT INTO test (comment) VALUES (@com)";
q.bind("@com", text);  // bind the TEXT
int err = q.exec();
```

Bind multiple values via auto index (?):
```c++
sqlitepp::query q(db);
q << "INSERT INTO test(name, data, comment) VALUES ('Test',?,?)";
std::vector<std::uint8_t> v(10U, 0x55U);
std::string t("A test text");
q.bind(1, v);
q.bind(2, t);
int err = q.exec();
```

Insert discret values via the << operator
```c++
sqlitepp::query q(db);
q << "INSERT INTO test (num, flo) VALUES(" << 1000 << "," << 3.1415F << ")";
int err = q.exec();
```

All text and string values need to be in UTF-8 format in SQLite.
Storing a string in UTF-8 - here on a windows platform with ATL conversion
```c++
sqlitepp::query q(db);
q << "INSERT INTO test(id, name) VALUES (13,'" << ATL::CT2CA(L"Schöne Grüße", CP_UTF8) << "')";
int err = q.exec();
```

Assembling a query out of different fragments via the << operator
```c++
sqlitepp::query q(db);
q << "UPDATE test SET num=";
q << 10;
q << " WHERE id=2";
int err = q.exec();
```

Sometime after exessive delete and insert operations it's useful to defragment/compact the database, which is done by the vacuum() command.
  // database defragmentation (e.g. after exessive deletes etc.)
```c++
int err = db.vacuum();
```

Get a complete result set
```c++
sqlitepp::query q(db, "SELECT * FROM test");
sqlitepp::result res = q.store();
```
Access single fields of the result set:
```c++
std::vector<std::uint8_t> blob;
std::string text;
int    a  = res[1]["num"];              // get data of num of row 1
double d  = res[0]["flo"];              // get data of flo of row 0
bool null = res[0]["num"].is_null();    // test if field num of row 0 is NULL
blob = res[0][4];                       // get blob data of row 0
text = res[2]["comment"];               // get text as string
```

Show all results which are not NULL
```c++
for (sqlitepp::result::size_type r = 0; r < res.num_rows(); ++r) {
  for (sqlitepp::row::size_type c = 0; c < res[r].num_fields(); ++c) {
    if (!res[r][c].is_null()) {
      std::cout << res[r][c].str() << " |";
    }
  }
  std::cout << std::endl;
}
```

Show all results, but access the result row by row.
This can be used if the result set is too big to be loaded entirely at once.
```c++
sqlitepp::query q(db, "SELECT * FROM test");
sqlitepp::row _row = q.use();
while (!_row.empty()) {
  for (sqlitepp::row::size_type c = 0; c < _row.num_fields(); ++c) {
    if (!_row[c].is_null()) {
      std::cout << _row[c].str() << " |";
    }
  }
  std::cout << std::endl;
  // get next row
  _row = q.use_next();
}
```

Access the result row by row, but abort after the first row. You MUST call use_abort().
This can be used if the result set is too big to be loaded entirely at once.
```c++
sqlitepp::query q(db, "SELECT * FROM test");
sqlitepp::row _row = q.use();
while (!_row.empty()) {
  for (sqlitepp::row::size_type c = 0; c < _row.num_fields(); ++c) {
    if (!_row[c].is_null()) {
      std::cout << _row[c].str() << " |";
    }
  }
  std::cout << std::endl;
  // abort here
  q.use_abort();
  break;
}
```

Evaluate the first row only
```c++
sqlitepp::query q(db, "SELECT * FROM test");
sqlitepp::row _row = q.use();
(void)q.use_abort();    // this is important - don't forget!
```

Start a transaction, begin() is called by the ctor
```c++
sqlitepp::transaction tr(db);
q.exec("INSERT INTO test(name) VALUES ('Marco')");
// commit
tr.commit();
```

Rolling back:
```c++
tr.begin();  // begin a new transaction
q.exec("INSERT INTO test(name) VALUES ('I'm not stored')");
// rollback
tr.rollback();
```

Rollback when the transaction goes out scope:
```c++
{
  sqlitepp::transaction tr(db);  // implicit begin
  q.exec("INSERT INTO test(name) VALUES ('I'm not stored')");
  // implicit rollback when tr goes out of scope here
}
```


## License
sqlitepp is LGPLv3
SQLite3 is public domain
