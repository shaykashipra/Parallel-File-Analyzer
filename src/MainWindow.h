#pragma once

#include "FileAnalysisResult.h"

#include <QMainWindow>
#include <QThreadPool>
#include <QStringList>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>

class QComboBox;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QLabel;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void chooseFolder();
    void discoverFiles();
    void startAnalysis();
    void cancelAnalysis();
    void handleAnalysisFinished(int row, const FileAnalysisResult& result);

private:
    struct Benchmark {
        QString signature;
        std::chrono::milliseconds elapsed{0};
    };

    void buildUi();
    void setRunning(bool running);
    void populateDiscoveredFiles();
    void prepareResultTable();
    void scheduleMoreWork();
    void markUnscheduledRowsCancelled();
    void updateTotals(const FileAnalysisResult& result);
    void updateProgress();
    void finishAnalysis();
    void updateBenchmark();
    [[nodiscard]] QString inputSignature() const;
    [[nodiscard]] QString numberText(std::size_t value) const;

    QThreadPool m_threadPool;
    QStringList m_files;
    QString m_selectedFolder;
    std::shared_ptr<std::atomic_bool> m_cancelRequested;
    std::chrono::steady_clock::time_point m_startedAt;
    int m_nextFileToSchedule = 0;
    int m_activeTaskCount = 0;
    int m_completedTaskCount = 0;
    int m_cancelledBeforeStartCount = 0;
    int m_concurrency = 1;
    bool m_running = false;
    std::size_t m_totalLines = 0;
    std::size_t m_totalWords = 0;
    std::size_t m_totalCharacters = 0;
    std::size_t m_totalKeywords = 0;
    std::optional<Benchmark> m_lastSequential;
    std::optional<Benchmark> m_lastParallel;

    QLineEdit* m_folderEdit = nullptr;
    QLineEdit* m_extensionsEdit = nullptr;
    QLineEdit* m_keywordEdit = nullptr;
    QComboBox* m_modeCombo = nullptr;
    QSpinBox* m_threadsSpinBox = nullptr;
    QPushButton* m_browseButton = nullptr;
    QPushButton* m_discoverButton = nullptr;
    QPushButton* m_startButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
    QTableWidget* m_discoveredTable = nullptr;
    QTableWidget* m_resultsTable = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_processedLabel = nullptr;
    QLabel* m_elapsedLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_totalsLabel = nullptr;
    QLabel* m_benchmarkLabel = nullptr;
};
