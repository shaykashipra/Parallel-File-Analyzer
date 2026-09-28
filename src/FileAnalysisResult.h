#pragma once

#include <QString>
#include <QMetaType>

#include <cstddef>

struct FileAnalysisResult {
    QString filePath;
    std::size_t lineCount = 0;
    std::size_t wordCount = 0;
    std::size_t characterCount = 0;
    std::size_t keywordCount = 0;
    QString status = QStringLiteral("Pending");

    [[nodiscard]] bool succeeded() const { return status == QStringLiteral("Success"); }
};

Q_DECLARE_METATYPE(FileAnalysisResult)
