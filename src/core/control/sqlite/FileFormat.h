#pragma once

#include <string>
#include "filesystem.h"

namespace xoj {

enum class FileFormat { XML, SQLITE };

/**
 * @brief Determina il formato del file in base all'estensione del percorso.
 */
inline FileFormat getFormatFromPath(const fs::path& path) {
    if (path.extension() == ".xoppj") {
        return FileFormat::SQLITE;
    }
    return FileFormat::XML;
}

} // namespace xoj