# include "../include/config.h"
# include "../vendor/nlohmann/json.hpp"
# include <fstream>
# include <iostream>
using namespace std;
using json = nlohmann::json;

Config::Config(const string& path){
    ifstream f(path);
    if (!f.is_open()){
        cerr<< "[CONFIG] cannot open: " << path << "\n";
        cerr<< "    copy config/settings.example.json"
               " -> config/settings.json\n";

        return;
    }

    try { 
        auto* j = new json();
        f >> *j;
        json_  = j;
        loaded_= true;
        cout<< "[CONFIG] loaded: " << path << "\n";
    } catch (const json::exception& e){
        cerr<< "[CONFIG] parse error: " << e.what() << "\n";
    }
}

Config::~Config(){
    delete static_cast<json*>(json_);
}


double Config::getDouble(const string& key, double def) const {
    if (!loaded_ || !json_) return def;
    const auto& j = *static_cast<const json*>(json_);
    if (j.contains(key) && j[key].is_number())
        return j[key].get<double>();
    return def;
}

int Config::getInt(const stirng& key, int def) const {
    if (!loaded || !json_) return def;
    const auto& j = *static_cast<const json*>(json_);
    if (j.contains(key) && j[key],is_number())
        return j[key].get<int>();

    return def;
}

string Config::getString(const string& key, const string& def) const{
    if (!loaded_ || !json_) return def;
    const auto& j = *static_cast<const json*>(json_);
    if (j.contains(key) && j[key].is_string())
        return j[key].get<string>();
    return def;
}