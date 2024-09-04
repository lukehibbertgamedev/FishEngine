#include "fish_utils.h"

std::string Fish::Utility::to_lower(const std::string& str)
{
    std::string result = str;
    std::string _return;
    for (char& c : result) {
        c = std::tolower(static_cast<unsigned char>(c));
        _return.push_back(c);
    }
    return _return;
}

std::string Fish::Utility::capitalise_first(const std::string& str)
{
    if (str.empty()) return str;
    std::string result = str;
    result[0] = std::toupper(result[0]);
    return result;
}

std::string Fish::Utility::extract_file_name(const std::string& filePath, bool format /* = true*/)
{
    // Find the last occurrence of '/' or '\\'
    size_t lastSlashPos = filePath.find_last_of("/\\");
    // Find the last occurrence of '.'
    size_t lastDotPos = filePath.find_last_of('.');

    // Extract the file name without the extension
    if (lastSlashPos == std::string::npos) {
        lastSlashPos = 0; // No slash found, use the start of the string
    }
    else {
        lastSlashPos++; // Start after the slash
    }

    // Ensure lastDotPos is after the lastSlashPos
    if (lastDotPos == std::string::npos || lastDotPos < lastSlashPos) {
        lastDotPos = filePath.length(); // No dot found, use the end of the string
    }

    std::string extracted = filePath.substr(lastSlashPos, lastDotPos - lastSlashPos);
    if (!format)
    {
        return extracted;
    }
    else
    {
        return capitalise_first(to_lower(extracted));
    }
}
