#include "FileAnalyzer.h"

#include <QFile>
#include <QTemporaryDir>

#include <atomic>
#include <cstdlib>
#include <iostream>

namespace {

bool writeUtf8File(const QString& path, const QByteArray& content)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

} // namespace

int main()
{
    QTemporaryDir directory;
    expect(directory.isValid(), "temporary directory should be created");
    const QString filePath = directory.filePath(QStringLiteral("sample.txt"));
    expect(writeUtf8File(filePath, "hello world\nhello Qt\n\xC3\xA9"), "test data should be written");

    std::atomic_bool notCancelled{false};
    const FileAnalysisResult result = FileAnalyzer::analyzeFile(filePath, QStringLiteral("hello"), notCancelled);
    expect(result.succeeded(), "valid UTF-8 file should succeed");
    expect(result.lineCount == 3, "line rule should count three logical lines");
    expect(result.wordCount == 5, "whitespace-separated word count should be five");
    expect(result.characterCount == 22, "UTF-8 e acute should count as one code point");
    expect(result.keywordCount == 2, "keyword matches should be case-sensitive and non-overlapping");

    std::atomic_bool cancelled{true};
    const FileAnalysisResult cancelledResult = FileAnalyzer::analyzeFile(filePath, QString(), cancelled);
    expect(cancelledResult.status == QStringLiteral("Cancelled"), "pre-cancelled work should not process a file");

    const FileAnalysisResult missing = FileAnalyzer::analyzeFile(directory.filePath(QStringLiteral("missing.txt")), QString(), notCancelled);
    expect(!missing.succeeded(), "missing file should return an error result");

    std::cout << "FileAnalyzerTests passed\n";
    return EXIT_SUCCESS;
}
