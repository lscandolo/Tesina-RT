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

typedef std::map<property_name_t, property_name_t> section_t;

class INIReader {

public:
        INIReader();
        int32_t load_file(std::string filename);
        int32_t get_str_value(section_name_t, property_name_t, std::string& ret_val);
        int32_t get_float_value(section_name_t, property_name_t, float& ret_val);
        int32_t get_int_value(section_name_t, property_name_t, int32_t& ret_val);

private:

        bool m_initialized;
        std::map<section_name_t, section_t> m_values;

};


#endif /* RT_INI_HPP */
