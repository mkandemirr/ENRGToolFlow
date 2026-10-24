#ifndef CONFIG_READER_H
#define CONFIG_READER_H

#include <string>
#include <vector>

class INIReader;

// Reads required keys from a Tool config file.
//
// Every Get...() call prints an error if the key is missing or cannot be parsed,
// and marks the reader invalid. A Tool reads all its keys first and then checks
// IsValid() once, so that every problem in the config is reported in one run.
class ConfigReader
{
public:
   explicit ConfigReader(const INIReader& iniReader) : fIniReader(iniReader) {}

   std::string GetString(const std::string& section, const std::string& key);
   long long GetInteger(const std::string& section, const std::string& key);
   double GetReal(const std::string& section, const std::string& key);
   bool GetBoolean(const std::string& section, const std::string& key);

   // Comma-separated list of integers, e.g. "0,1,2,3". Must not be empty.
   std::vector<int> GetIntegerList(const std::string& section, const std::string& key);

   bool IsValid() const { return fValid; }

private:
   bool GetRaw(const std::string& section, const std::string& key, std::string& value);
   void Invalid(const std::string& section, const std::string& key,
                const std::string& text, const std::string& expected);

   const INIReader& fIniReader;
   bool fValid = true;
};

#endif
