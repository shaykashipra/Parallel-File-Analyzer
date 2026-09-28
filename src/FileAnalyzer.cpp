#include "FileAnalyzer.h"

#include <QFile>
#include <QString>

namespace {

bool isCancelled(const std::atomic_bool& cancelRequested)
{
    return cancelRequested.load(std::memory_order_relaxed);
}

} // namespace

FileAnalysisResult FileAnalyzer::analyzeFile(const QString& filePath,
                                             const QString& keyword,
                                             const std::atomic_bool& cancelRequested)
{
    FileAnalysisResult result;
    result.filePath = filePath;

    if (isCancelled(cancelRequested)) {
        result.status = QStringLiteral("Cancelled");
        return result;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.status = QStringLiteral("Error: %1").arg(file.errorString());
        return result;
    }

    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        result.status = QStringLiteral("Error: %1").arg(file.errorString());
        return result;
    }

    if (isCancelled(cancelRequested)) {
        result.status = QStringLiteral("Cancelled");
        return result;
    }

    // Qt replaces malformed UTF-8 sequences with U+FFFD. This keeps analysis safe and predictable.
    QString text = QString::fromUtf8(bytes);
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    if (!text.isEmpty()) {
        result.lineCount = static_cast<std::size_t>(text.count(QLatin1Char('\n')));
        if (!text.endsWith(QLatin1Char('\n'))) {
            ++result.lineCount;
        }
    }

    bool insideWord = false;
    for (const char32_t codePoint : text.toUcs4()) {
        if (isCancelled(cancelRequested)) {
            result.status = QStringLiteral("Cancelled");
            return result;
        }

        ++result.characterCount;
        const bool whitespace = QChar::isSpace(static_cast<uint>(codePoint));
        if (!whitespace && !insideWord) {
            ++result.wordCount;
        }
        insideWord = !whitespace;
    }

    if (!keyword.isEmpty()) {
        qsizetype position = 0;
        while (true) {
            if (isCancelled(cancelRequested)) {
                result.status = QStringLiteral("Cancelled");
                return result;
            }

            position = text.indexOf(keyword, position, Qt::CaseSensitive);
            if (position < 0) {
                break;
            }
            ++result.keywordCount;
            position += keyword.size(); // Count non-overlapping, case-sensitive matches.
        }
    }

    result.status = QStringLiteral("Success");
    return result;
}
