// Mainly this file is to implement constructor and create tables
# include "../include/database.h"
# include <iostream>
using namespace std;

DatabaseHandler::DatabaseHandler(const string& dbPath){
    int rc = sqlite3_open(dbPath.c_str(), &db_);
    if (rc != SQLITE_OK){
        cerr<< "[DB] cann't open: "
            << sqlite3_errmsg(db_) << "\n";
        sqlite3_close(db_);
        db_ = nullptr;
        return;
    }

    cout<< "[DB] Opneed: " << dbPath << "\n";
    createTables();
}

DatabaseHandler::~DatabaseHandler(){
    if (db_){
        sqlite3_close(db_);
        cout<< "[DB] connection closed. \n";
    }
}

bool DatabaseHandler::execSQL(const string& sql){
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);     // here was error as sql.c_str is a function pointer. It needs to be sql.c_str()

    if (rc!= SQLITE_OK){                                                    // its SQLITE_OK not SQLITE3_OK
        cerr << "[DB] SQL error: " << errMsg << "\n";
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool DatabaseHandler::createTables(){
    const char* sql = R"SQL(
        CREATE TABLE IF NOT EXISTS orders (
            orderID     INTEGER PRIMARY KEY,
            userID      TEXT    NOT NULL,
            price       REAL    NOT NULL,
            quantity    INTEGER NOT NULL,
            side        TEXT    NOT NULL,
            timestamp   INTEGER NOT NULL,
            status      TEXT    NOT NULL DEFAULT 'PENDING',
            blockScore  REAL             DEFAULT 0.0,
            fraudFlag   INTEGER          DEFAULT 0   
        );
        CREATE TABLE IF NOT EXISTS trades(
            tradeID     INTEGER PRIMARY KEY AUTOINCREMENT,
            buyOrderID  INTEGER NOT NULL,
            sellOrderID INTEGER NOT NULL,
            price       REAL    NOT NULL,
            quantity    INTEGER NOT NULL,
            timestamp   INTEGER NOT NULL,
            FOREIGN KEY (buyOrderID)    REFERENCES orders(orderID),
            FOREIGN KEY (sellOrderID)   REFERENCES orders(orderID)     
        );
        CREATE TABLE IF NOT EXISTS risk_log(
            logID       INTEGER PRIMARY KEY AUTOINCREMENT,
            userID      TEXT    NOT NULL,
            orderID     INTEGER NOT NULL,
            fraudScore  REAL    NOT NULL,
            reason      TEXT    NOT NULL,
            action      TEXT    NOT NULL,
            timestamp   INTEGER NOT NULL,
            FOREIGN KEY (orderID) REFERENCES orders(orderID)               // theres s in orders which i missed
        );
    )SQL";  

    bool ok = execSQL(sql);
    if (ok) cout << "[DB] Tables ready. \n";
    return ok;
}
