#pragma once

#include "FileAnalysisResult.h"

#include <atomic>

class QString;

class FileAnalyzer {
public:
    // Reads UTF-8 text and returns either Success, Cancelled, or a readable error status.
    static FileAnalysisResult analyzeFile(const QString& filePath,
                                          const QString& keyword,
                                          const std::atomic_bool& cancelRequested);
};
