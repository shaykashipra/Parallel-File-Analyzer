#include "AnalysisWorker.h"

#include "FileAnalyzer.h"

#include <utility>

AnalysisWorker::AnalysisWorker(int row,
                               QString filePath,
                               QString keyword,
                               std::shared_ptr<std::atomic_bool> cancelRequested)
    : m_row(row)
    , m_filePath(std::move(filePath))
    , m_keyword(std::move(keyword))
    , m_cancelRequested(std::move(cancelRequested))
{
    setAutoDelete(true);
}

void AnalysisWorker::run()
{
    const FileAnalysisResult result = FileAnalyzer::analyzeFile(
        m_filePath, m_keyword, *m_cancelRequested);
    emit analysisFinished(m_row, result);
}
