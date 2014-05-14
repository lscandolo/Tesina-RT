#pragma once
#ifndef RT_INI_HPP
#define RT_INI_HPP

#include <string>
#include <map>
#include <vector>
#include <stdint.h>

typedef std::string section_name_t;
typedef std::string property_name_t;
typedef std::string value_t;

typedef std::map<property_name_t, value_t> section_t;

class INIReader {

public:
        INIReader();
        int32_t load_file(std::string filename);
        int32_t get_str_value(section_name_t, property_name_t, std::string& ret_val);
        int32_t get_float_value(section_name_t, property_name_t, float& ret_val);
        int32_t get_int_value(section_name_t, property_name_t, int32_t& ret_val);

        std::vector<std::string> get_str_list(section_name_t, property_name_t);
        std::vector<float>       get_float_list(section_name_t, property_name_t);
        std::vector<int>         get_int_list(section_name_t, property_name_t);
        std::vector<bool>        get_bool_list(section_name_t, property_name_t);

private:

        // input string are assumed to be trimmed (front and back)
        std::vector<std::string> get_str_list(std::string);
        std::vector<float>       get_float_list(std::string);
        std::vector<int>         get_int_list(std::string);
        std::vector<bool>        get_bool_list(std::string);

        bool m_initialized;
        std::map<section_name_t, section_t> m_values;

};


#endif /* RT_INI_HPP */
