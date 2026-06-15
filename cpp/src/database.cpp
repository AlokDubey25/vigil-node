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

    // Turning Foreign key ON to avoid confusion of not existing rows
    execSQL("PRAGMA journal_mode = WAL;");               // concurrent reads + writes
    execSQL("PRAGMA synchronous = NORMAL;");            // safe but faster than full
    execSQL("PRAGMA foreign_keys = ON;");              // already there from before
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
            FOREIGN KEY (orderID) REFERENCES orders(orderID)               -- theres s in orders which i missed
        );
    )SQL";  

    bool ok = execSQL(sql);
    if (ok) cout << "[DB] Tables ready. \n";
    return ok;
}

bool DatabaseHandler::saveTrade(Trade& t){
    sqlite3_stmt* stmt = nullptr;

    // tradeID is AUTOINCREMENT so no need to insert
    const char* sql =
        "INSERT INTO trades"
        " (buyOrderID, sellOrderID, price, quantity, timestamp)"
        "VALUES (?,?,?,?,?)";

    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK){
        cerr<< "[DB] saveTrade prepare: " << sqlite3_errmsg(db_)<< "\n";
        return false;
    }

    sqlite3_bind_int    (stmt, 1, t.buyOrderID);
    sqlite3_bind_int    (stmt, 2, t.sellOrderID);
    sqlite3_bind_double (stmt, 3, t.price);
    sqlite3_bind_int    (stmt, 4, t.quantity);
    sqlite3_bind_int64  (stmt, 5, t.timestamp);

    rc = sqlite3_step(stmt);



/* 
    bool ok = (rc == SQLITE_DONE);
    if (!ok){                   // This here fails whenever it failed (ok == false)
        // grab the tradeID the DB just assigned
        t.tradeID = static_cast<int>(sqlite3_last_insert_rowid(db_));
        sqlite3_finalize(stmt);
        return true;        // SO this returns true on failure of ok
    }else{                 // And this blocks runs if it was SUCESSFULL (ok == true)
        cerr<< "[DB] saveTrade step failded: "<< sqlite3_errmsg(db_)<< "\n";
        sqlite3_finalize(stmt);
        return false;   // Returning False on sucess
    }
*/

    // Changing conditional laytout
    if (rc == SQLITE_DONE){
        // success pathway: Extract new autoincremented key identity
        t.tradeID = static_cast<int>(sqlite3_last_insert_rowid(db_));
        sqlite3_finalize(stmt);
        return true;
    } else {
        // Failure Pathway
        cerr << "[DB] saveTrade step failed: " << sqlite3_errmsg(db_) << "\n";
        sqlite3_finalize(stmt);
        return false;
    }
    
}


bool DatabaseHandler::saveRiskEvent(
    const string& userID, int orderID,
    double score,
    const string& reason,
    const string& action)
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO risk_log"
        " (userID, orderID, fraudScore, reason, action, timestamp)"
        "VALUES (?,?,?,?,?,?)";

    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK){
        cerr<< "[DB] saveRiskEvent prepare: "<< sqlite3_errmsg(db_)<<"\n";
        return false;
    }

    sqlite3_bind_text   (stmt, 1, userID.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int    (stmt, 2, orderID);
    sqlite3_bind_double (stmt, 3, score);
    sqlite3_bind_text   (stmt, 4 ,reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text   (stmt, 5, action.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64  (stmt, 6, static_cast<long long>(time(nullptr)));

    rc = sqlite3_step(stmt);
    bool ok = (rc == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}


bool DatabaseHandler::updateOrderStatus(int orderID, const string& status){
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE orders SET status=? WHERE orderID=?";

    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK){
        cerr<< "[DB] updateStatus prepare: "<< sqlite3_errmsg(db_)<< "\n";
        return false;
    }

    sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 2, orderID);

    rc = sqlite3_step(stmt);
    bool ok = (rc == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}


// Now this is to read blacklisted users using SQLITE_ROW loops
vector<string> DatabaseHandler::loadBlacklist(){
    vector<string> blocked;

    if (!db_) return blocked;   // DB not open - nothing to load 


    sqlite3_stmt* stmt = nullptr;
    
    const char* sql = "SELECT DISTINCT userID FROM orders WHERE fraudFlag = 1";
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK){
        cerr<< "[DB] loadBlacklist prepare: "
            << sqlite3_errmsg(db_) << "\n";
        return blocked;    
    }
        // loop sqlite3_step until it stops by providing rows to us
    while (sqlite3_step(stmt) == SQLITE_ROW){
        // clmn index is 0 (opposite of bind which is 1)
        const unsigned char* raw = sqlite3_column_text(stmt, 0);
        if (raw){
            // convert uchar* -> char* here...
            blocked.emplace_back(reinterpret_cast<const char*>(raw));
        }
    }
        
    sqlite3_finalize(stmt);
    cout<< "[DB] loaded " << blocked.size()
        << " permanently blocked users\n";

    return blocked;
    
}

bool DatabaseHandler::saveOrder(const Order& order) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = 
        "INSERT INTO orders (orderID, userID, price, quantity, side, timestamp, status, blockScore, fraudFlag)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";

    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        cerr << "[DB] saveOrder prepare error: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }

    sqlite3_bind_int   (stmt, 1, order.orderID);
    sqlite3_bind_text  (stmt, 2, order.userID.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, order.price);
    sqlite3_bind_int   (stmt, 4, order.quantity);
    sqlite3_bind_text  (stmt, 5, order.side.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64 (stmt, 6, order.timestamp);
    sqlite3_bind_text  (stmt, 7, order.status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 8, order.blockScore);
    sqlite3_bind_int   (stmt, 9, order.fraudFlag);

    rc = sqlite3_step(stmt);
    bool ok = (rc == SQLITE_DONE);

    if (!ok) {
        cerr << "[DB] saveOrder step error: " << sqlite3_errmsg(db_) << "\n";
    }

    sqlite3_finalize(stmt);
    return ok;
}