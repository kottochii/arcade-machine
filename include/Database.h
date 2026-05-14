#ifndef ARCADE_MACHINE_DATABASE_H
#define ARCADE_MACHINE_DATABASE_H

// TODO: figure out a way to keep persistent connections for a while

#include <iostream>
#include <string>
#include <vector>
#include "splashkit.h"
#include "Table.h"
#include <map>
#include <tuple>
#include <sqlite3.h>
#include <memory>

#define DB_OPEN_READONLY true
#define DB_OPEN_READWRITE false

class Database {
    private:
        std::string m_databaseFileName;
        std::vector<Table*> m_tables;

    public:

        /**
         * Opens a connection to the database. 
         * If this returns true (ie. the connection has been established), the sqlite3* pointer will have to be properly disposed of by sqlite3_close_v2
         */
        bool open_db (sqlite3** _db, bool readonly, int* err_code)
        {
            int flags = SQLITE_OPEN_CREATE;

            // known issue: readonly access is not working
            // if(!readonly)
                flags |= SQLITE_OPEN_READWRITE;
                
            int rc = sqlite3_open_v2(m_databaseFileName.c_str(), _db, flags, nullptr);
            if (err_code) {
                *err_code = rc;
            }
            return rc == SQLITE_OK;
        }
    
        // Constructors
        Database()
            : m_databaseFileName ("arcadeMachine.db")
        {
            sqlite3* m_db;
            bool success = open_db(&m_db, DB_OPEN_READWRITE, nullptr);
            if(!success)
            {
                std::cerr << "Failed to open database: " << sqlite3_errmsg(m_db) << std::endl;
            }
            else
            {
                sqlite3_close_v2(m_db);
            }
        };

        Database(std::string databaseFileName)
            : m_databaseFileName (databaseFileName)
        {
            sqlite3* m_db;
            bool success = open_db(&m_db, DB_OPEN_READWRITE, nullptr);
            if(!success)
            {
                std::cerr << "Failed to open database: " << sqlite3_errmsg(m_db) << std::endl;
            }
            else
            {
                sqlite3_close_v2(m_db);
            }
        };

        ~Database()
        {
            std::cout << "Destructor called on database: \"" << m_databaseFileName << "\"\n";
            std::cout << "Database: Clearing table memory...\n";
            for (auto& table : m_tables) delete table;
            m_tables.clear();
        }

        // Getters
        std::string getDatabaseFileName(){
            return m_databaseFileName;
        };

        std::vector<Table*> getTables(){
            return m_tables;
        };


        // Methods

        // Creates a new table if it doesn't exist
        bool createTable(Table *table){
            sqlite3* m_db;
            bool success = open_db(&m_db, DB_OPEN_READWRITE, nullptr);
            if(!success)
            {
                std::cerr << "Failed to open database: " << sqlite3_errmsg(m_db) << std::endl;
                return false;
            }

            // Add the table to the Databases vector of tables
            m_tables.push_back(table);

            // Create the query
            std::string createTable = "CREATE TABLE IF NOT EXISTS " + table->getTableName() + " (";
            std::map<std::string, std::string> columnNames = table->getColumnNames();
            for (auto const& pair : columnNames) {
                createTable += pair.first + " " + pair.second + ", ";
            }
            createTable = createTable.substr(0, createTable.size() - 2);
            createTable += ")";

            // Execute the query
            sqlite3_stmt* stmt;
            int rc = sqlite3_prepare_v2(m_db, createTable.c_str(), -1, &stmt, nullptr);
            if(rc != SQLITE_OK) {
                std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
                sqlite3_close_v2(m_db);
                return false;
            }
            rc = sqlite3_step(stmt);
            if(rc != SQLITE_DONE) {
                std::cerr << "Failed to execute statement: " << sqlite3_errmsg(m_db) << std::endl;
                sqlite3_finalize(stmt);
                sqlite3_close_v2(m_db);
                return false;
            }

            sqlite3_finalize(stmt);
            sqlite3_close_v2(m_db);

            std::cout << "Table \"" << table->getTableName() << "\" created successfully" << std::endl;
            return true;
        };

        // Inserts a new row into a table
        bool insertData(std::string tableName, std::map<std::string, std::string> data){
            // Check if table exists
            bool exists = std::get<0>(hasTable(tableName));

            if (exists) {
                sqlite3* m_db;
                bool success = open_db(&m_db, DB_OPEN_READWRITE, nullptr);
                if(!success)
                {
                    std::cerr << "Failed to open database: " << sqlite3_errmsg(m_db) << std::endl;
                    return false;
                }

                // Create the query
                std::string insertData = "INSERT INTO " + tableName + " (";
                for (auto const& pair : data) {
                    insertData += pair.first + ", ";
                }
                insertData = insertData.substr(0, insertData.size() - 2);
                insertData += ") VALUES (";

                for (auto const& pair : data) {
                    insertData += "'" + pair.second + "', ";
                }
                insertData = insertData.substr(0, insertData.size() - 2);
                insertData += ")";

                // Execute the query
                sqlite3_stmt* stmt;
                int rc = sqlite3_prepare_v2(m_db, insertData.c_str(), -1, &stmt, nullptr);
                if(rc != SQLITE_OK) {
                    std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
                    sqlite3_close_v2(m_db);
                    return false;
                }
                rc = sqlite3_step(stmt);
                if(rc != SQLITE_DONE) {
                    std::cerr << "Failed to execute statement: " << sqlite3_errmsg(m_db) << std::endl;
                    sqlite3_finalize(stmt);
                    sqlite3_close_v2(m_db);
                    return false;
                }

                sqlite3_finalize(stmt);
                sqlite3_close_v2(m_db);
                std::cout << "Data inserted successfully" << std::endl;
                return true;


            } else { // Return false if the table does not exist
                std::cout << "Table does not exist" << std::endl;
                return false;
            }
        };


        // Returns a vector of vectors of strings containing the data in the table
        std::vector<std::vector<std::string>> getAllData(std::string tableName){
            // Check if table exists
            std::tuple<bool, Table*> tableExists = hasTable(tableName);

            bool exists = std::get<0>(tableExists);

            Table *table = std::get<1>(tableExists);

            if (exists) {
                std::vector<std::vector<string>> data;
                sqlite3* m_db;
                bool success = open_db(&m_db, DB_OPEN_READONLY, nullptr);
                if(!success)
                {
                    std::cerr << "Failed to open database: " << sqlite3_errmsg(m_db) << std::endl;
                    return std::vector<std::vector<std::string>>();
                }

                // Create the query
                string query = "SELECT * FROM " + tableName;

                // Execute the query
                sqlite3_stmt* stmt;
                int rc = sqlite3_prepare_v2(m_db, query.c_str(), -1, &stmt, nullptr);
                if(rc != SQLITE_OK) {
                    std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
                    sqlite3_close_v2(m_db);
                    return std::vector<std::vector<std::string>>();
                }

                // Add the column names to the data vector first
                std::vector<std::string> columnNames;
                for (auto const& pair: table->getColumnNames()) {
                    columnNames.push_back(pair.first);
                }
                data.push_back(columnNames);

                // Add the data to the data vector
                int last_result;
                while ((last_result = sqlite3_step(stmt)) == SQLITE_ROW) {
                    // Get each row as a vector of strings
                    std::vector<std::string> row = get_current_row_strings(stmt);

                    data.push_back(row);
                }

                if(last_result != SQLITE_DONE) {
                    std::cerr << "Failed to execute statement, still returning data: " << sqlite3_errmsg(m_db) << std::endl;
                    sqlite3_finalize(stmt);
                    sqlite3_close_v2(m_db);
                    return data;
                }

                sqlite3_finalize(stmt);
                sqlite3_close_v2(m_db);

                // Return the data
                std::cout << "Data returned successfully" << std::endl;
                return data;

            } else { // Return an empty vector if the table does not exist
                std::cout << "Table does not exist" << std::endl;
                return std::vector<std::vector<std::string>>() ;
            }
        };

        // Prints the all data in the table to the console
        bool printAllData(std::string tableName){
            // Check if table exists
            std::tuple<bool, Table*> tableExists = hasTable(tableName);

            bool exists = std::get<0>(tableExists);

            Table *table = std::get<1>(tableExists);


            if (exists) {

                sqlite3* m_db;
                bool success = open_db(&m_db, DB_OPEN_READONLY, nullptr);
                if(!success)
                {
                    std::cerr << "Failed to open database: " << sqlite3_errmsg(m_db) << std::endl;
                    return false;
                }

                // Create the query
                string query = "SELECT * FROM " + tableName;

                // Execute the query
                sqlite3_stmt* stmt;
                int rc = sqlite3_prepare_v2(m_db, query.c_str(), -1, &stmt, nullptr);
                if(rc != SQLITE_OK) {
                    std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
                    sqlite3_close_v2(m_db);
                    return false;
                }

                // Print the column names
                for (auto const& pair: table->getColumnNames()) {
                    std::cout << pair.first  // string (key)
                            << ':'
                            << pair.second
                            << " | "; // string's value
                }
                std::cout << std::endl;

                // Print the data
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    std::vector<std::string> row = get_current_row_strings(stmt);

                    for (int i = 0; i < row.size(); i++) {
                        std::cout << row[i] << " | ";
                    }
                    std::cout << std::endl;
                }
                sqlite3_finalize(stmt);
                sqlite3_close_v2(m_db);

                // Return true if the data was printed
                std::cout << "Data printed successfully" << std::endl;
                return true;

            } else { // Return false if the table does not exist
                std::cout << "Table does not exist" << std::endl;
                return false;
            }
        };

        // Returns a bool if a row was deleted
        bool deleteData(std::string tableName, std::map<std::string, std::string> data){
            // Check if table exists
            bool exists = std::get<0>(hasTable(tableName));

            if (exists) {
                sqlite3* m_db;
                bool success = open_db(&m_db, DB_OPEN_READWRITE, nullptr);
                if(!success)                {
                    std::cerr << "Failed to open database: " << sqlite3_errmsg(m_db) << std::endl;
                    return false;
                }

                // Create the query
                std::string query = "DELETE FROM " + tableName + " WHERE ";
                for (auto const& pair : data) {
                    query += pair.first + " = '" + pair.second + "' AND ";
                }
                query = query.substr(0, query.size() - 5);
                query += ";";

                // Execute the query
                sqlite3_stmt* stmt;
                int rc = sqlite3_prepare_v2(m_db, query.c_str(), -1, &stmt, nullptr);
                if(rc != SQLITE_OK) {
                    std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
                    sqlite3_close_v2(m_db);
                    return false;
                }
                rc = sqlite3_step(stmt);
                if(rc != SQLITE_DONE) {
                    std::cerr << "Failed to execute statement: " << sqlite3_errmsg(m_db) << std::endl;
                    sqlite3_finalize(stmt);
                    sqlite3_close_v2(m_db);
                    return false;
                }

                sqlite3_finalize(stmt);
                sqlite3_close_v2(m_db);
                return true;

            } else { //     Return false if the table does not exist
                std::cout << "Table does not exist" << std::endl;
                return false;
            }

        };

        // Returns a bool if a row was updated
        // The newData parameter is a map of column names to new values
        // The conditionData parameter is a map of column names to values to be used in the WHERE clause

        bool updateData(std::string tableName, std::map<std::string, std::string> newData, std::map<std::string, std::string> conditionData){
            // Check if table exists
            bool exists = std::get<0>(hasTable(tableName));

            if (exists) {
                sqlite3* m_db;
                bool success = open_db(&m_db, DB_OPEN_READWRITE, nullptr);
                if(!success)                {
                    std::cerr << "Failed to open database: " << sqlite3_errmsg(m_db) << std::endl;
                    return false;
                }

                // Create the query
                std::string query = "UPDATE " + tableName + " SET ";
                for (auto const& pair : newData) {
                    query += pair.first + " = '" + pair.second + "', ";
                }
                query = query.substr(0, query.size() - 2);
                query += " WHERE ";
                for (auto const& pair : conditionData) {
                    query += pair.first + " = '" + pair.second + "' AND ";
                }
                query = query.substr(0, query.size() - 5);
                query += ";";

                // Execute the query
                sqlite3_stmt* stmt;
                int rc = sqlite3_prepare_v2(m_db, query.c_str(), -1, &stmt, nullptr);
                if(rc != SQLITE_OK) {
                    std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
                    sqlite3_close_v2(m_db);
                    return false;
                }
                rc = sqlite3_step(stmt);
                if(rc != SQLITE_DONE) {
                    std::cerr << "Failed to execute statement: " << sqlite3_errmsg(m_db) << std::endl;
                    sqlite3_finalize(stmt);
                    sqlite3_close_v2(m_db);
                    return false;
                }

                // Return true if the row was updated
                std::cout << "Data updated successfully" << std::endl;
                sqlite3_finalize(stmt);
                sqlite3_close_v2(m_db);
                return true;
            } else {   // Return false if the table does not exist
                std::cout << "Table does not exist" << std::endl;
                return false;
            }
        };

        // Returns a bool if a table was dropped
        bool dropTable(std::string tableName){
            // Check if table exists
            bool exists = std::get<0>(hasTable(tableName));
            if (exists) {
                sqlite3* m_db;
                bool success = open_db(&m_db, DB_OPEN_READWRITE, nullptr);
                if(!success)                {
                    std::cerr << "Failed to open database: " << sqlite3_errmsg(m_db) << std::endl;
                    return false;
                }
                // Create the query
                std::string query = "DROP TABLE " + tableName;
                // Execute the query
                sqlite3_stmt* stmt;
                int rc = sqlite3_prepare_v2(m_db, query.c_str(), -1, &stmt, nullptr);
                if(rc != SQLITE_OK) {
                    std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
                    sqlite3_close_v2(m_db);
                    return false;
                }
                rc = sqlite3_step(stmt);
                if(rc != SQLITE_DONE) {
                    std::cerr << "Failed to execute statement: " << sqlite3_errmsg(m_db) << std::endl;
                    sqlite3_finalize(stmt);
                    sqlite3_close_v2(m_db);
                    return false;
                }
                sqlite3_finalize(stmt);
                sqlite3_close_v2(m_db);
                return true;

            } else { // Return false if the table does not exist
                std::cout << "Table does not exist" << std::endl;
                return false;
            }
        };


        // Returns a vector of vectors of strings containing the data matching a condition
        // The conditionData parameter is a map of column names to values to be used in the WHERE clause
        std::vector<std::vector<std::string>> getData(std::string tableName, std::map<std::string, std::string> conditionData){
            // Check if table exists
            std::tuple<bool, Table*> tableExists = hasTable(tableName);

            bool exists = std::get<0>(tableExists);

            Table *table = std::get<1>(tableExists);

            if (exists) {
                std::vector<std::vector<std::string>> data;
                sqlite3* m_db;
                bool success = open_db(&m_db, DB_OPEN_READONLY, nullptr);
                if(!success) {
                    std::cerr << "Failed to open database: " << sqlite3_errmsg(m_db) << std::endl;
                    return std::vector<std::vector<std::string>>();
                }

                // Create the query
                std::string query = "SELECT * FROM " + tableName + " WHERE ";
                for (auto const& pair : conditionData) {
                    query += pair.first + " = '" + pair.second + "' AND ";
                }
                query = query.substr(0, query.size() - 5);
                query += ";";
                // Execute the query
                sqlite3_stmt* stmt;
                int rc = sqlite3_prepare_v2(m_db, query.c_str(), -1, &stmt, nullptr);
                if(rc != SQLITE_OK) {
                    std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
                    sqlite3_close_v2(m_db);
                    return std::vector<std::vector<std::string>>();
                }

                // Add the column names to the data vector first
                std::vector<std::string> columnNames;
                for (auto const& pair: table->getColumnNames()) {
                    columnNames.push_back(pair.first);
                }
                data.push_back(columnNames);

                // Add the data to the data vector
                int last_step;
                while ((last_step = sqlite3_step(stmt)) == SQLITE_ROW) {
                    // Get each row as a vector of strings
                    std::vector<std::string> row = get_current_row_strings(stmt);

                    data.push_back(row);

                }

                // Return the data
                if (last_step == SQLITE_DONE) {
                    std::cout << "Data returned successfully" << std::endl;
                } else {
                    std::cout << "Data return failed" << std::endl;
                    std::cout << query << std::endl;
                }
                sqlite3_finalize(stmt);
                sqlite3_close_v2(m_db);
                return data;
            } else { // Return an empty vector if the table does not exist
                std::cout << "Table does not exist" << std::endl;
                return std::vector<std::vector<string>>();
            }
        };

        // Returns the current row as a vector of strings
        std::vector<std::string> get_current_row_strings(sqlite3_stmt* stmt) {
            std::vector<std::string> row;
            for (int i = 0; i < sqlite3_column_count(stmt); i++) {
                row.push_back(std::string{reinterpret_cast<const char*>(sqlite3_column_text(stmt, i))});
            }
            return row;
        }

        // Returns a tuple of a bool and a table if the table exists
        std::tuple<bool, Table*> hasTable(std::string tableName){

            // Check if table exists
            std::tuple<bool, Table*> exists;

            for (int i= 0; i < m_tables.size(); i++){
                if (m_tables[i]->getTableName() == tableName) {
                    exists = std::make_tuple(true, m_tables[i]);
                    return exists;
                }
            }

            std::cout << "Table does not exist" << std::endl;
            exists = std::make_tuple(false, nullptr);
            return exists;
        };
};


#endif
