#include <misc/ini.hpp>

#include <fstream>
#include <iostream>
#include <cstdlib>

static void remove_excess_spaces(std::string& line) 
{
        int start_pos = 0, end_pos = line.length()-1;
        while (line[start_pos] == ' ' || line[start_pos] == '\t')
                start_pos++;
        while (line[end_pos] == ' ' || line[end_pos] == '\t')
                end_pos--;
        if (start_pos <= end_pos)
                line = line.substr(start_pos, end_pos - start_pos + 1);
        else
                line.clear();
        return;
}

static int get_next_token(std::string& str, std::string& token,
                          int start_pos, int& end_pos) 
{
        while (start_pos < str.size() && 
               (str[start_pos] == ' ' || str[start_pos] == '\t')) {
                start_pos++;
        }
        if (start_pos >= str.size())
                return -1;

        end_pos = start_pos;
        while (end_pos < str.size() && 
               (str[end_pos] != ' ' && str[end_pos] != '\t')) {
                end_pos++;
        }
        token = str.substr(start_pos, end_pos - start_pos + 1);
        return 0;
}

INIReader::INIReader() 
{
        m_initialized = false;
}

int32_t 
INIReader::load_file(std::string filename)
{
        std::ifstream file;
        file.open(filename.c_str(),std::ios_base::in);
        if (!file.good())
                return 1;

        section_name_t current_section;
        std::string line;

        size_t line_number = 0;

        while(!file.eof()) {
                line_number++;
                std::getline(file, line);
                remove_excess_spaces(line);

                // Comment or empty line
                if (!line.length() || line[0] == ';') {
                        continue;
                }

                // Section name
                if (line[0] == '[') {
                        size_t end_pos = line.find(']');
                        if (end_pos == std::string::npos || end_pos == 1)
                                return -(int)line_number;
                        current_section = line.substr(1,end_pos-1);
                        continue;
                }
                
                // Property = value line
                size_t eq_pos = line.find('=');
                if (eq_pos == std::string::npos) {
                        return -(int)line_number;
                }
                std::string name = line.substr(0, eq_pos);
                std::string val  = line.substr(eq_pos+1, line.length() - eq_pos - 1);

                remove_excess_spaces(name);
                remove_excess_spaces(val);

                if (name.length() == 0 || val.length() == 0) {
                        return -(int)line_number;
                }
                m_values[current_section][name] = val;

        }
        return 0;
}

int32_t
INIReader::get_str_value(section_name_t section_name, 
                         property_name_t property_name,
                         std::string& ret_val)
{
        if (m_values.find(section_name) == m_values.end())
                return -1;
        section_t& section = m_values[section_name];
        if (section.find(property_name) == section.end())
                return -1;

        ret_val = section[property_name];
        return 0;
}

int32_t
INIReader::get_float_value(section_name_t section_name, 
                           property_name_t property_name,
                           float& ret_val)
{
        if (m_values.find(section_name) == m_values.end())
                return -1;
        section_t& section = m_values[section_name];
        if (section.find(property_name) == section.end())
                return -1;

        ret_val = std::atof(section[property_name].c_str());
        return 0;
}

int32_t
INIReader::get_int_value(section_name_t section_name, 
                         property_name_t property_name,
                         int32_t& ret_val)
{
        if (m_values.find(section_name) == m_values.end())
                return -1;
        section_t& section = m_values[section_name];
        if (section.find(property_name) == section.end())
                return -1;

        ret_val = std::atoi(section[property_name].c_str());
        return 0;
}

std::vector<std::string>
INIReader::get_str_list(section_name_t section_name, 
                        property_name_t property_name)
{
        std::vector<std::string> lst;
        if (m_values.find(section_name) == m_values.end())
                return lst;
        section_t& section = m_values[section_name];
        if (section.find(property_name) == section.end())
                return lst;
        
        return get_str_list(section[property_name]);
}

std::vector<std::string>
INIReader::get_str_list(std::string str)
{
        std::vector<std::string> lst;
        std::string token;
        int start_pos = 0;
        int end_pos;
        while (!get_next_token(str, token, start_pos, end_pos)) {
                start_pos = end_pos;
                lst.push_back(token);
        }
        return lst;
}

std::vector<float>
INIReader::get_float_list(section_name_t section_name, 
                          property_name_t property_name)
{
        std::vector<float> lst;
        if (m_values.find(section_name) == m_values.end())
                return lst;
        section_t& section = m_values[section_name];
        if (section.find(property_name) == section.end())
                return lst;
        
        return get_float_list(section[property_name]);
}

std::vector<float>
INIReader::get_float_list(std::string str)
{
        std::vector<float> lst;
        std::string token;
        int start_pos = 0;
        int end_pos;
        while (!get_next_token(str, token, start_pos, end_pos)) {
                start_pos = end_pos;
                lst.push_back(std::atof(token.c_str()));
        }
        return lst;
}

std::vector<int>
INIReader::get_int_list(section_name_t section_name, 
                          property_name_t property_name)
{
        std::vector<int> lst;
        if (m_values.find(section_name) == m_values.end())
                return lst;
        section_t& section = m_values[section_name];
        if (section.find(property_name) == section.end())
                return lst;
        
        return get_int_list(section[property_name]);
}

std::vector<int>
INIReader::get_int_list(std::string str)
{
        std::vector<int> lst;
        std::string token;
        int start_pos = 0;
        int end_pos;
        while (!get_next_token(str, token, start_pos, end_pos)) {
                start_pos = end_pos;
                lst.push_back(std::atoi(token.c_str()));
        }
        return lst;
}

std::vector<bool>
INIReader::get_bool_list(section_name_t section_name, 
                          property_name_t property_name)
{
        std::vector<bool> lst;
        if (m_values.find(section_name) == m_values.end())
                return lst;
        section_t& section = m_values[section_name];
        if (section.find(property_name) == section.end())
                return lst;
        
        return get_bool_list(section[property_name]);
}

std::vector<bool>
INIReader::get_bool_list(std::string str)
{
        std::vector<bool> lst;
        std::string token;
        int start_pos = 0;
        int end_pos;
        while (!get_next_token(str, token, start_pos, end_pos)) {
                start_pos = end_pos;
                lst.push_back(std::atoi(token.c_str()));
        }
        return lst;
}

