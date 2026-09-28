#pragma once

#include "FileAnalysisResult.h"

#include <QObject>
#include <QRunnable>

#include <atomic>
#include <memory>

class AnalysisWorker final : public QObject, public QRunnable {
    Q_OBJECT

public:
    AnalysisWorker(int row,
                   QString filePath,
                   QString keyword,
                   std::shared_ptr<std::atomic_bool> cancelRequested);

    void run() override;

signals:
    void analysisFinished(int row, const FileAnalysisResult& result);

private:
    int m_row;
    QString m_filePath;
    QString m_keyword;
    std::shared_ptr<std::atomic_bool> m_cancelRequested;
};
