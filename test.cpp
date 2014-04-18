///////////////////////////////////////////////////////////////////////////////
/// \author (c) Marco Paland (marco@paland.com)
///             2014, PALANDesign Hannover, Germany
///
/// \license LGPLv3
/// This file is part of the sqlitepp project.
/// sqlitepp is free software: you can redistribute it and/or modify
/// it under the terms of the GNU Lesser Public License as published by
/// the Free Software Foundation, either version 3 of the License, or
/// (at your option) any later version.
/// sqlitepp is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
/// GNU Lesser Public License for more details.
/// You should have received a copy of the GNU Lesser Public License
/// along with sqlitepp. If not, see <http://www.gnu.org/licenses/>.
///
///
/// \brief sqlitepp examples for function/access demonstration
///
///////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <iostream>
#include <AtlBase.h>    // for string conversion to UTF-8 in Windows

#include "sqlitepp/sqlitepp.h"

// defines the test database file - here in memory
#define TEST_DB ":memory:"


int main()
{
  sqlitepp::db db(TEST_DB);
  assert(db.is_open());
  std::cout << "test - SQLite3  version: " << db.version() << std::endl;
  std::cout << "test - sqlitepp version: " << SQLITEPP_VERSION << std::endl;

  int err;

  // query ctor test
  sqlitepp::query qc(db, "DROP TABLE test;");
  err = qc.exec();
  std::cout << "test - query ctor: " << err << std::endl;

  // query with exec test
  sqlitepp::query q(db, "THIS QUERY SHOULD GET DISCARDED");
  err = q.exec("CREATE TABLE test (id INTEGER PRIMARY KEY NOT NULL, num INTEGER, name VARCHAR(20), flo FLOAT, data BLOB, comment TEXT);");
  std::cout << "test - query exec: " << err << std::endl;

  // insert BLOB via bind
  std::vector<std::uint8_t> blob(30U);
  for (size_t i = 0; i < blob.size(); ++i) { blob[i] = static_cast<std::uint8_t>(i); }
  q.bind(1, blob);
  err = q.exec("INSERT INTO test (data) VALUES (?1)");
  std::cout << "test - insert BLOB via bind: " << err << ", id: " << q.insert_id() << std::endl;

  // insert BLOB via operator binding
  q << "INSERT INTO test (data) VALUES (?)" << blob;
  err = q.exec();
  std::cout << "test - insert BLOB via <<: " << err << ", id: " << q.insert_id() << std::endl;

  // insert text, use alpha index
  std::string text("Test");
  q << "INSERT INTO test (comment) VALUES (@com)";
  q.bind("@com", text);
  err = q.exec();
  std::cout << "test - insert TEXT via alpha bind: " << err << ", id: " << q.insert_id() << std::endl;

  // bind multiple values
  q << "INSERT INTO test(name, data, comment) VALUES ('Test',?,?)";
  std::vector<std::uint8_t> v(10U, 0x55U);
  std::string t("A test text");
  q.bind(1, v);
  q.bind(2, t);
  err = q.exec();
  std::cout << "test - insert multiple binds: " << err << ", id: " << q.insert_id() << std::endl;

  // insert discret values
  q << "INSERT INTO test (num, flo) VALUES(" << 1000 << "," << 3.1415F << ")";
  err = q.exec();
  std::cout << "test - insert: " << err << ", id: " << q.insert_id() << std::endl;

  // store a string in UTF-8 - here on a windows platform with ATL conversion
  q << "INSERT INTO test(id, name) VALUES (13,'" << ATL::CT2CA(L"Schöne Grüße", CP_UTF8) << "')";
  err = q.exec();
  std::cout << "test - insert: " << err << ", id: " << q.insert_id() << ", affected rows: " << q.affected_rows() << std::endl;

  // query assembly
  q << "UPDATE test SET num=";
  q << 10;
  q << " WHERE id=2";
  err = q.exec();
  std::cout << "test - update: " << err << ", affected rows: " << q.affected_rows() << std::endl;

  // database defragmentation (e.g. after exessive deletes etc.)
  err = db.vacuum();
  std::cout << "test - defragmentation: " << err << std::endl;

  // access results
  q << "SELECT * FROM test";
  sqlitepp::result res = q.store();
  std::cout << "test - result: Got " << res.num_rows() << " rows" << std::endl;

  // access of single fields
  int    a  = res[1]["num"];              // get data of num of row 1
  double d  = res[0]["flo"];              // get data of flo of row 0
  bool null = res[0]["num"].is_null();    // test if field is NULL
  blob = res[0][4];                       // get blob data of row 0
  text = res[2]["comment"];               // get text as string

  // show all results which are not NULL
  for (sqlitepp::result::size_type r = 0; r < res.num_rows(); ++r) {
    for (sqlitepp::row::size_type c = 0; c < res[r].num_fields(); ++c) {
      if (!res[r][c].is_null()) {
        std::cout << (std::string)res[r][c] << " |";
      }
    }
    std::cout << std::endl;
  }

  // same, but access row by row
  q << "SELECT * FROM test";
  sqlitepp::row _row = q.use();
  while (!_row.empty()) {
    for (sqlitepp::row::size_type c = 0; c < _row.num_fields(); ++c) {
      if (!_row[c].is_null()) {
        std::cout << (std::string)_row[c] << " |";
      }
    }
    std::cout << std::endl;
    // get next row
    _row = q.use_next();
  }

  // evaluate the first row only
  q << "SELECT * FROM test";
  _row = q.use();
  (void)q.use_abort();    // this is important - don't forget!

  // start a transaction (implicit begin)
  sqlitepp::transaction tr(db);
  q.exec("INSERT INTO test(name) VALUES ('Marco')");
  // commit
  tr.commit();

  tr.begin();
  q.exec("INSERT INTO test(name) VALUES ('I'm not stored')");
  // rollback
  tr.rollback();

  {
    sqlitepp::transaction tr2(db);  // implicit begin
    q.exec("INSERT INTO test(name) VALUES ('I'm not stored either')");
    // implicit rollback when tr2 goes out of scope here
  }

  return 0;
}
