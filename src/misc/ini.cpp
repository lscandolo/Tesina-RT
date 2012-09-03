#include <misc/ini.hpp>

#include <fstream>
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
                        int end_pos = line.find(']');
                        if (end_pos == std::string::npos || end_pos == 1)
                                return -line_number;
                        current_section = line.substr(1,end_pos-1);
                        continue;
                }
                
                // Property = value line
                int eq_pos = line.find('=');
                if (eq_pos == std::string::npos) {
                        return -line_number;
                }
                std::string name = line.substr(0, eq_pos);
                std::string val  = line.substr(eq_pos+1, line.length() - eq_pos - 1);

                remove_excess_spaces(name);
                remove_excess_spaces(val);

                if (name.length() == 0 || val.length() == 0) {
                        return -line_number;
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
