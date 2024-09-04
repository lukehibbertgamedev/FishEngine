#pragma once

#include <string>

namespace Fish {

    namespace Utility {
        std::string to_lower(const std::string& str);

        std::string capitalise_first(const std::string& str);

        // Helper function to extract a nicely formatted name from a filepath. (../assets/hOusE.glb will return House)
        std::string extract_file_name(const std::string& filePath, bool format = true);
    }
}