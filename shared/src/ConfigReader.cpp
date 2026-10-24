#include "ConfigReader.h"

#include <cstdlib>
#include <iostream>
#include <sstream>

#include "INIReader.h"

bool ConfigReader::GetRaw(const std::string& section, const std::string& key,
                          std::string& value)
{
   const std::string missing = "\x01<missing>";
   value = fIniReader.Get(section, key, missing);

   if (value == missing)
   {
      std::cerr << "Error: missing config key [" << section << "] " << key << std::endl;
      fValid = false;
      return false;
   }

   return true;
}

void ConfigReader::Invalid(const std::string& section, const std::string& key,
                           const std::string& text, const std::string& expected)
{
   std::cerr << "Error: config key [" << section << "] " << key << " must be "
             << expected << ": '" << text << "'" << std::endl;
   fValid = false;
}

std::string ConfigReader::GetString(const std::string& section, const std::string& key)
{
   std::string value;
   GetRaw(section, key, value);
   return value;
}

long long ConfigReader::GetInteger(const std::string& section, const std::string& key)
{
   std::string text;

   if (!GetRaw(section, key, text))
   {
      return 0;
   }

   char* end = nullptr;
   const long long value = std::strtoll(text.c_str(), &end, 10);

   if (text.empty() || *end != '\0')
   {
      Invalid(section, key, text, "an integer");
      return 0;
   }

   return value;
}

double ConfigReader::GetReal(const std::string& section, const std::string& key)
{
   std::string text;

   if (!GetRaw(section, key, text))
   {
      return 0;
   }

   char* end = nullptr;
   const double value = std::strtod(text.c_str(), &end);

   if (text.empty() || *end != '\0')
   {
      Invalid(section, key, text, "a number");
      return 0;
   }

   return value;
}

bool ConfigReader::GetBoolean(const std::string& section, const std::string& key)
{
   std::string text;

   if (!GetRaw(section, key, text))
   {
      return false;
   }

   if (text == "true")
   {
      return true;
   }

   if (text != "false")
   {
      Invalid(section, key, text, "true or false");
   }

   return false;
}

std::vector<int> ConfigReader::GetIntegerList(const std::string& section,
                                              const std::string& key)
{
   std::vector<int> values;
   std::string text;

   if (!GetRaw(section, key, text))
   {
      return values;
   }

   std::stringstream stream(text);
   std::string item;

   while (std::getline(stream, item, ','))
   {
      char* end = nullptr;
      const long value = std::strtol(item.c_str(), &end, 10);

      if (item.empty() || *end != '\0')
      {
         Invalid(section, key, text, "a comma-separated list of integers");
         return {};
      }

      values.push_back(value);
   }

   if (values.empty())
   {
      Invalid(section, key, text, "a non-empty list");
   }

   return values;
}
