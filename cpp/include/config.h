# pragma once
# include <string>
using namespace std;

clss Config{
public:
    // opens n parses setting.json
    // if file missing, falls back to defaults silently
    explicit Config(const string& path);
    ~Config();

    bool isLoaded() const { return loaded_;}

    // read with fallback - never throws
    double getDouble(const string& key, double def = 0.0)  const;
    int    getInt   (const string& key, int    def = 0  )  const;
    string getString(const string& key, const  def = "" )  const;


private:
    void* json_ = nullptr;  // pimpl: nlohmann::json allocated on heap
    bool loaded_= false;
};