#include "MainWindow.h"

#include "AnalysisWorker.h"

#include <QAbstractItemView>
#include <QCloseEvent>
#include <QComboBox>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QThread>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int FileNameColumn = 0;
constexpr int LinesColumn = 1;
constexpr int WordsColumn = 2;
constexpr int CharactersColumn = 3;
constexpr int KeywordsColumn = 4;
constexpr int StatusColumn = 5;

QString elapsedText(std::chrono::milliseconds elapsed)
{
    return QStringLiteral("%1 ms").arg(elapsed.count());
}

class NumericTableWidgetItem final : public QTableWidgetItem {
public:
    NumericTableWidgetItem()
        : QTableWidgetItem(QStringLiteral("-"))
    {
    }

    void setValue(std::size_t value)
    {
        m_value = value;
        setText(QLocale().toString(static_cast<qulonglong>(value)));
    }

    bool operator<(const QTableWidgetItem& other) const override
    {
        if (const auto* numericOther = dynamic_cast<const NumericTableWidgetItem*>(&other)) {
            return m_value < numericOther->m_value;
        }
        return QTableWidgetItem::operator<(other);
    }

private:
    std::size_t m_value = 0;
};

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    qRegisterMetaType<FileAnalysisResult>("FileAnalysisResult");
    m_threadPool.setExpiryTimeout(-1);
    buildUi();
    setWindowTitle(QStringLiteral("Parallel File Analyzer"));
    resize(1100, 760);
}

MainWindow::~MainWindow()
{
    m_threadPool.waitForDone();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_running) {
        m_cancelRequested->store(true, std::memory_order_relaxed);
        m_threadPool.waitForDone();
    }
    event->accept();
}

void MainWindow::buildUi()
{
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);

    auto* inputGroup = new QGroupBox(QStringLiteral("Analysis Setup"), central);
    auto* inputLayout = new QGridLayout(inputGroup);
    m_folderEdit = new QLineEdit(inputGroup);
    m_folderEdit->setReadOnly(true);
    m_browseButton = new QPushButton(QStringLiteral("Browse..."), inputGroup);
    m_extensionsEdit = new QLineEdit(QStringLiteral(".txt"), inputGroup);
    m_extensionsEdit->setPlaceholderText(QStringLiteral(".txt, .log, .md"));
    m_keywordEdit = new QLineEdit(inputGroup);
    m_keywordEdit->setPlaceholderText(QStringLiteral("Optional, case-sensitive"));
    m_modeCombo = new QComboBox(inputGroup);
    m_modeCombo->addItems({QStringLiteral("Sequential"), QStringLiteral("Parallel")});
    m_threadsSpinBox = new QSpinBox(inputGroup);
    m_threadsSpinBox->setRange(1, 64);
    m_threadsSpinBox->setValue(std::max(1, QThread::idealThreadCount()));
    m_discoverButton = new QPushButton(QStringLiteral("Find Files"), inputGroup);
    m_startButton = new QPushButton(QStringLiteral("Start Analysis"), inputGroup);
    m_cancelButton = new QPushButton(QStringLiteral("Cancel"), inputGroup);
    m_cancelButton->setEnabled(false);

    inputLayout->addWidget(new QLabel(QStringLiteral("Folder:"), inputGroup), 0, 0);
    inputLayout->addWidget(m_folderEdit, 0, 1, 1, 4);
    inputLayout->addWidget(m_browseButton, 0, 5);
    inputLayout->addWidget(new QLabel(QStringLiteral("Extensions:"), inputGroup), 1, 0);
    inputLayout->addWidget(m_extensionsEdit, 1, 1);
    inputLayout->addWidget(new QLabel(QStringLiteral("Keyword:"), inputGroup), 1, 2);
    inputLayout->addWidget(m_keywordEdit, 1, 3);
    inputLayout->addWidget(new QLabel(QStringLiteral("Mode:"), inputGroup), 1, 4);
    inputLayout->addWidget(m_modeCombo, 1, 5);
    inputLayout->addWidget(new QLabel(QStringLiteral("Parallel workers:"), inputGroup), 2, 0);
    inputLayout->addWidget(m_threadsSpinBox, 2, 1);
    inputLayout->addWidget(m_discoverButton, 2, 3);
    inputLayout->addWidget(m_startButton, 2, 4);
    inputLayout->addWidget(m_cancelButton, 2, 5);
    layout->addWidget(inputGroup);

    m_discoveredTable = new QTableWidget(central);
    m_discoveredTable->setColumnCount(2);
    m_discoveredTable->setHorizontalHeaderLabels({QStringLiteral("File"), QStringLiteral("Path")});
    m_discoveredTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_discoveredTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_discoveredTable->horizontalHeader()->setStretchLastSection(true);

    m_resultsTable = new QTableWidget(central);
    m_resultsTable->setColumnCount(6);
    m_resultsTable->setHorizontalHeaderLabels({QStringLiteral("File"), QStringLiteral("Lines"),
                                                QStringLiteral("Words"), QStringLiteral("Characters"),
                                                QStringLiteral("Keyword Matches"), QStringLiteral("Status")});
    m_resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsTable->setSortingEnabled(true);
    m_resultsTable->horizontalHeader()->setStretchLastSection(true);

    auto* splitter = new QSplitter(Qt::Vertical, central);
    auto* discoveredGroup = new QGroupBox(QStringLiteral("Discovered Files"), splitter);
    auto* discoveredLayout = new QVBoxLayout(discoveredGroup);
    discoveredLayout->addWidget(m_discoveredTable);
    auto* resultsGroup = new QGroupBox(QStringLiteral("Analysis Results"), splitter);
    auto* resultsLayout = new QVBoxLayout(resultsGroup);
    resultsLayout->addWidget(m_resultsTable);
    splitter->addWidget(discoveredGroup);
    splitter->addWidget(resultsGroup);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter, 1);

    auto* progressLayout = new QGridLayout;
    m_progressBar = new QProgressBar(central);
    m_processedLabel = new QLabel(QStringLiteral("Processed: 0 / 0"), central);
    m_elapsedLabel = new QLabel(QStringLiteral("Elapsed: -"), central);
    m_statusLabel = new QLabel(QStringLiteral("Choose a folder, then find files."), central);
    m_totalsLabel = new QLabel(QStringLiteral("Totals: lines 0 | words 0 | characters 0 | keyword matches 0"), central);
    m_benchmarkLabel = new QLabel(QStringLiteral("Benchmark: run both modes on the same inputs to compare."), central);
    progressLayout->addWidget(m_progressBar, 0, 0, 1, 3);
    progressLayout->addWidget(m_processedLabel, 1, 0);
    progressLayout->addWidget(m_elapsedLabel, 1, 1);
    progressLayout->addWidget(m_statusLabel, 1, 2);
    progressLayout->addWidget(m_totalsLabel, 2, 0, 1, 3);
    progressLayout->addWidget(m_benchmarkLabel, 3, 0, 1, 3);
    layout->addLayout(progressLayout);

    setCentralWidget(central);
    statusBar()->showMessage(QStringLiteral("Ready"));

    connect(m_browseButton, &QPushButton::clicked, this, &MainWindow::chooseFolder);
    connect(m_discoverButton, &QPushButton::clicked, this, &MainWindow::discoverFiles);
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::startAnalysis);
    connect(m_cancelButton, &QPushButton::clicked, this, &MainWindow::cancelAnalysis);
    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, [this] {
        m_threadsSpinBox->setEnabled(m_modeCombo->currentText() == QStringLiteral("Parallel"));
    });
}

void MainWindow::chooseFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, QStringLiteral("Select Folder"), m_selectedFolder);
    if (!folder.isEmpty()) {
        m_selectedFolder = folder;
        m_folderEdit->setText(folder);
    }
}

void MainWindow::discoverFiles()
{
    if (m_selectedFolder.isEmpty() || !QFileInfo(m_selectedFolder).isDir()) {
        QMessageBox::warning(this, QStringLiteral("Invalid Folder"), QStringLiteral("Select a valid folder first."));
        return;
    }

    QStringList extensions;
    for (const QString& rawExtension : m_extensionsEdit->text().split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        QString extension = rawExtension.trimmed().toLower();
        if (!extension.isEmpty()) {
            if (!extension.startsWith(QLatin1Char('.'))) {
                extension.prepend(QLatin1Char('.'));
            }
            extensions.append(extension);
        }
    }
    extensions.removeDuplicates();
    if (extensions.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("No Extensions"), QStringLiteral("Enter at least one text-file extension."));
        return;
    }

    m_files.clear();
    QDirIterator iterator(m_selectedFolder, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        const QString extension = QStringLiteral(".") + QFileInfo(path).suffix().toLower();
        if (extensions.contains(extension)) {
            m_files.append(path);
        }
    }
    std::sort(m_files.begin(), m_files.end());
    populateDiscoveredFiles();
    m_resultsTable->setRowCount(0);
    m_statusLabel->setText(QStringLiteral("Found %1 matching file(s).").arg(m_files.size()));
    statusBar()->showMessage(QStringLiteral("Discovery complete"));
}

void MainWindow::populateDiscoveredFiles()
{
    m_discoveredTable->setRowCount(m_files.size());
    for (qsizetype row = 0; row < m_files.size(); ++row) {
        const QFileInfo info(m_files.at(row));
        m_discoveredTable->setItem(row, 0, new QTableWidgetItem(info.fileName()));
        m_discoveredTable->setItem(row, 1, new QTableWidgetItem(info.absoluteFilePath()));
    }
}

void MainWindow::startAnalysis()
{
    if (m_files.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("No Files"), QStringLiteral("Find matching files before starting analysis."));
        return;
    }

    m_running = true;
    m_cancelRequested = std::make_shared<std::atomic_bool>(false);
    m_nextFileToSchedule = 0;
    m_activeTaskCount = 0;
    m_completedTaskCount = 0;
    m_cancelledBeforeStartCount = 0;
    m_totalLines = m_totalWords = m_totalCharacters = m_totalKeywords = 0;
    m_concurrency = m_modeCombo->currentText() == QStringLiteral("Parallel") ? m_threadsSpinBox->value() : 1;
    m_threadPool.setMaxThreadCount(m_concurrency);
    m_startedAt = std::chrono::steady_clock::now();
    prepareResultTable();
    setRunning(true);
    scheduleMoreWork();
}

void MainWindow::prepareResultTable()
{
    m_resultsTable->setSortingEnabled(false);
    m_resultsTable->setRowCount(m_files.size());
    for (qsizetype row = 0; row < m_files.size(); ++row) {
        const QFileInfo info(m_files.at(row));
        m_resultsTable->setItem(row, FileNameColumn, new QTableWidgetItem(info.fileName()));
        for (int column = LinesColumn; column <= KeywordsColumn; ++column) {
            m_resultsTable->setItem(row, column, new NumericTableWidgetItem);
        }
        m_resultsTable->setItem(row, StatusColumn, new QTableWidgetItem(QStringLiteral("Pending")));
    }
}

void MainWindow::scheduleMoreWork()
{
    while (!m_cancelRequested->load(std::memory_order_relaxed)
           && m_activeTaskCount < m_concurrency
           && m_nextFileToSchedule < m_files.size()) {
        const int row = m_nextFileToSchedule++;
        auto* worker = new AnalysisWorker(row, m_files.at(row), m_keywordEdit->text(), m_cancelRequested);
        connect(worker, &AnalysisWorker::analysisFinished, this, &MainWindow::handleAnalysisFinished,
                Qt::QueuedConnection);
        ++m_activeTaskCount;
        m_threadPool.start(worker);
    }

    if (m_cancelRequested->load(std::memory_order_relaxed)) {
        markUnscheduledRowsCancelled();
    }
}

void MainWindow::cancelAnalysis()
{
    if (!m_running) {
        return;
    }
    m_cancelRequested->store(true, std::memory_order_relaxed);
    markUnscheduledRowsCancelled();
    m_statusLabel->setText(QStringLiteral("Cancellation requested. Finishing active file reads safely..."));
    m_cancelButton->setEnabled(false);
}

void MainWindow::markUnscheduledRowsCancelled()
{
    while (m_nextFileToSchedule < m_files.size()) {
        const int row = m_nextFileToSchedule++;
        m_resultsTable->item(row, StatusColumn)->setText(QStringLiteral("Cancelled"));
        ++m_cancelledBeforeStartCount;
    }
    updateProgress();
}

void MainWindow::handleAnalysisFinished(int row, const FileAnalysisResult& result)
{
    --m_activeTaskCount;
    ++m_completedTaskCount;
    static_cast<NumericTableWidgetItem*>(m_resultsTable->item(row, LinesColumn))->setValue(result.lineCount);
    static_cast<NumericTableWidgetItem*>(m_resultsTable->item(row, WordsColumn))->setValue(result.wordCount);
    static_cast<NumericTableWidgetItem*>(m_resultsTable->item(row, CharactersColumn))->setValue(result.characterCount);
    static_cast<NumericTableWidgetItem*>(m_resultsTable->item(row, KeywordsColumn))->setValue(result.keywordCount);
    m_resultsTable->item(row, StatusColumn)->setText(result.status);
    updateTotals(result);
    updateProgress();

    if (!m_cancelRequested->load(std::memory_order_relaxed)) {
        scheduleMoreWork();
    }
    if (m_activeTaskCount == 0 && (m_nextFileToSchedule >= m_files.size()
                                   || m_cancelRequested->load(std::memory_order_relaxed))) {
        finishAnalysis();
    }
}

void MainWindow::updateTotals(const FileAnalysisResult& result)
{
    if (!result.succeeded()) {
        return;
    }
    m_totalLines += result.lineCount;
    m_totalWords += result.wordCount;
    m_totalCharacters += result.characterCount;
    m_totalKeywords += result.keywordCount;
    m_totalsLabel->setText(QStringLiteral("Totals: lines %1 | words %2 | characters %3 | keyword matches %4")
                               .arg(numberText(m_totalLines), numberText(m_totalWords),
                                    numberText(m_totalCharacters), numberText(m_totalKeywords)));
}

void MainWindow::updateProgress()
{
    m_progressBar->setRange(0, m_files.size());
    m_progressBar->setValue(m_completedTaskCount + m_cancelledBeforeStartCount);
    m_processedLabel->setText(QStringLiteral("Processed: %1 / %2")
                                  .arg(m_completedTaskCount)
                                  .arg(m_files.size()));
    m_elapsedLabel->setText(QStringLiteral("Elapsed: %1")
                                .arg(elapsedText(std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - m_startedAt))));
}

void MainWindow::finishAnalysis()
{
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - m_startedAt);
    updateProgress();
    m_elapsedLabel->setText(QStringLiteral("Elapsed: %1").arg(elapsedText(elapsed)));
    const bool cancelled = m_cancelRequested->load(std::memory_order_relaxed);
    m_statusLabel->setText(cancelled ? QStringLiteral("Cancelled safely.") : QStringLiteral("Completed."));
    statusBar()->showMessage(cancelled ? QStringLiteral("Analysis cancelled") : QStringLiteral("Analysis complete"));
    m_resultsTable->setSortingEnabled(true);

    if (!cancelled) {
        Benchmark benchmark{inputSignature(), elapsed};
        if (m_modeCombo->currentText() == QStringLiteral("Sequential")) {
            m_lastSequential = benchmark;
        } else {
            m_lastParallel = benchmark;
        }
    }
    updateBenchmark();
    m_running = false;
    setRunning(false);
}

void MainWindow::updateBenchmark()
{
    if (!m_lastSequential || !m_lastParallel || m_lastSequential->signature != m_lastParallel->signature) {
        m_benchmarkLabel->setText(QStringLiteral("Benchmark: run both modes on the same inputs to compare."));
        return;
    }
    if (m_lastParallel->elapsed.count() <= 0) {
        m_benchmarkLabel->setText(QStringLiteral("Benchmark: parallel time was too small to calculate a valid speedup."));
        return;
    }
    const double speedup = static_cast<double>(m_lastSequential->elapsed.count())
        / static_cast<double>(m_lastParallel->elapsed.count());
    m_benchmarkLabel->setText(QStringLiteral("Benchmark: sequential %1 | parallel %2 | speedup %3x")
                                  .arg(elapsedText(m_lastSequential->elapsed), elapsedText(m_lastParallel->elapsed))
                                  .arg(QString::number(speedup, 'f', 2)));
}

QString MainWindow::inputSignature() const
{
    return m_files.join(QChar(0x1F)) + QChar(0x1E) + m_keywordEdit->text();
}

QString MainWindow::numberText(std::size_t value) const
{
    return QLocale().toString(static_cast<qulonglong>(value));
}

void MainWindow::setRunning(bool running)
{
    m_browseButton->setEnabled(!running);
    m_discoverButton->setEnabled(!running);
    m_extensionsEdit->setEnabled(!running);
    m_keywordEdit->setEnabled(!running);
    m_modeCombo->setEnabled(!running);
    m_threadsSpinBox->setEnabled(!running && m_modeCombo->currentText() == QStringLiteral("Parallel"));
    m_startButton->setEnabled(!running);
    m_cancelButton->setEnabled(running);
}
