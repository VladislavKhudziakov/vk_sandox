#include "base_file.hpp"

using namespace sandbox::hal::filesystem;


cannot_open_file_error::cannot_open_file_error(const std::string& err_msg)
    : m_err_msg(err_msg)
{
}


const char* cannot_open_file_error::what() const
{
    return m_err_msg.c_str();
}