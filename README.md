# Parallel File Analyzer

Parallel File Analyzer is a C++17 and Qt 6 Widgets desktop application for inspecting text files in a selected folder. It discovers files by extension, measures lines, words, Unicode code points, and case-sensitive keyword occurrences, then compares sequential and parallel processing times.

The project is intentionally compact and study-friendly. It demonstrates a responsive desktop UI, modern C++ ownership, Qt signals and slots, `QThreadPool`/`QRunnable`, atomic cancellation, file I/O, STL algorithms, and `std::chrono` benchmarking without extra frameworks.

## Features

- Folder picker and recursive discovery of configurable extensions (`.txt` by default).
- Discovered-file table and sortable analysis-results table.
- Per-file line, word, character, keyword-match, and error/cancellation status.
- Sequential mode (one background worker at a time) and parallel mode (a selected number of workers).
- Progress, processed-file count, elapsed time, successful-file totals, and benchmark comparison.
- Safe cancellation: no new work is scheduled after cancellation and active workers observe a shared atomic flag.
- Graceful handling for missing, unreadable, empty, malformed UTF-8, and read-error files.

## Screenshots

No screenshots are invented for this repository. After building the program, capture the main window with a discovered-file list and completed results, then add image files under `docs/screenshots/` and reference them here, for example:

```md
![Completed analysis](docs/screenshots/completed-analysis.png)
```

## Counting Rules

- Files are read as UTF-8. Invalid byte sequences are replaced by Qt with U+FFFD (replacement character), so the application remains safe rather than claiming the file was perfectly valid text.
- A **character** is one Unicode code point, calculated with `QString::toUcs4()`. It is not a UTF-8 byte count and not a user-perceived grapheme-cluster count. For example, a letter plus a combining accent may count as two code points.
- A **word** is a maximal run of non-whitespace Unicode code points. Punctuation is part of a word if it is not separated by whitespace.
- Line endings are normalized so `LF`, `CRLF`, and `CR` have the same behavior. Lines are counted as newline separators plus a final non-terminated segment; an empty file has zero lines, and `a\n` has one line.
- Keyword matches are exact, case-sensitive, and non-overlapping. An empty keyword reports zero matches.

## Technology and Concepts

- C++17, Qt 6 Core, and Qt 6 Widgets.
- CMake and Qt's CMake helper functions.
- `QThreadPool` and `QRunnable` for bounded concurrent file work.
- Signals and slots with `Qt::QueuedConnection` to marshal worker results back to the GUI thread.
- `std::shared_ptr<std::atomic_bool>` for a small shared cancellation token.
- `QStringList`, `std::sort`, `std::optional`, and `std::chrono`.
- RAII through stack objects such as `QFile` and automatic worker deletion by `QRunnable`.

## Prerequisites: Windows

This workspace was inspected on 2026-09-28. It has GCC 14.2.0 available, but `cmake`, `qmake`, `qtpaths6`, and `C:\Qt` were not found. Therefore this checkout has not been built here yet.

Install a compatible Qt 6 kit before building:

1. Install [Qt Online Installer](https://www.qt.io/download-qt-installer) and choose Qt 6.8 or newer.
2. Select one matching desktop kit, such as **MSVC 2022 64-bit** or **MinGW 64-bit**. Do not mix a MinGW Qt kit with an MSVC compiler.
3. In the installer, select the kit's CMake support and Qt Creator, or install current CMake separately from [cmake.org](https://cmake.org/download/).
4. Restart the terminal after installation. `cmake --version` should work, and the Qt kit directory should contain `lib/cmake/Qt6`.

### Build with Qt Creator

1. Open `CMakeLists.txt` in Qt Creator.
2. Select the installed Qt 6 desktop kit.
3. Choose **Build > Build Project**.
4. Run the `ParallelFileAnalyzer` target.
5. Run the `FileAnalyzerTests` target or use Qt Creator's test pane.

### Build from a command prompt

Open a terminal configured for the matching compiler. Replace the example Qt path with your actual kit path.

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:\Qt\6.8.0\mingw_64"
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\ParallelFileAnalyzer.exe
```

For a single-config generator such as Ninja or MinGW Makefiles, the executable commonly appears as `build\ParallelFileAnalyzer.exe`. For Visual Studio, it commonly appears as `build\Debug\ParallelFileAnalyzer.exe`.

## Example Usage

1. Start the application and select a folder.
2. Keep `.txt` or enter extensions such as `.txt, .log, .md`.
3. Select **Find Files**. Discovery searches subdirectories.
4. Optionally enter `TODO` as a keyword.
5. Choose **Sequential** and start analysis. Record the elapsed time.
6. With the same files and keyword, choose **Parallel**, choose a worker count, and run again. The benchmark label becomes valid only when both completed runs have the same input signature.
7. Click column headers to sort the completed result table. Click Cancel to prevent remaining work from being scheduled.

## Project Layout

```text
Parallel File Analyzer/
├── CMakeLists.txt                 # Qt targets and CTest setup
├── src/
│   ├── main.cpp                   # QApplication entry point
│   ├── MainWindow.h/.cpp          # Widgets, workflow, UI-thread updates
│   ├── FileAnalyzer.h/.cpp        # Pure per-file counting logic
│   ├── FileAnalysisResult.h       # Result data passed from worker to UI
│   └── AnalysisWorker.h/.cpp      # QRunnable worker and completion signal
├── tests/FileAnalyzerTests.cpp    # Dependency-free core logic checks
├── README.md
├── INTERVIEW_PREPARATION.md
└── LEARNING_GUIDE.md
```

## How Processing Works

`MainWindow::startAnalysis()` prepares one result row per file, captures a shared cancellation token, starts the clock, and schedules up to the allowed concurrency. Each `AnalysisWorker` calls `FileAnalyzer::analyzeFile()` on a pool thread. The worker emits `analysisFinished`; Qt queues that signal to `MainWindow::handleAnalysisFinished()`, where table cells and totals are updated on the GUI thread.

Sequential mode uses the same code path with a concurrency limit of one. This preserves UI responsiveness while making the scheduling behavior easy to compare. Parallel mode schedules only up to the selected pool capacity at once; each completion schedules the next file.

## Thread Safety and Cancellation

Workers do not touch widgets. Their mutable work is local to `FileAnalyzer::analyzeFile()`. The only intentionally shared state is a `std::atomic_bool` cancellation flag. `Cancel` sets it, stops further scheduling, marks untouched rows cancelled, and active workers check it before and during count loops. Worker results are passed by value in queued Qt signals. During shutdown, the window requests cancellation and waits for its owned pool to finish, avoiding worker access to a destroyed window.

## Benchmarking and Limits

Elapsed wall-clock time comes from `std::chrono::steady_clock`. Speedup is `sequential_ms / parallel_ms` and is shown only after completed sequential and parallel runs have the same file-path list and keyword. A speedup value is a measurement for that run, not a promise. Storage type, OS caching, file sizes, file count, CPU cores, encoding conversion, and other running programs all influence it. Parallel work may be slower for a small number of files or an I/O-bound disk.

Known limitations:

- Discovery itself currently runs in the GUI thread, so an extremely large directory tree can briefly delay the UI. File analysis stays asynchronous.
- Each file is read into memory before counting, which is simple and appropriate for study-sized text files but not ideal for huge files.
- The application treats configured extensions as text; it does not detect formats or encodings automatically.
- Benchmark signatures do not detect a file that changes contents between two runs.

## Troubleshooting

- **`Could not find Qt6`**: pass the correct `-DCMAKE_PREFIX_PATH` pointing to the root of the chosen Qt kit, or configure the kit in Qt Creator.
- **Compiler/linker mismatch**: use the compiler distributed with or compatible with the selected Qt kit. A MinGW Qt kit needs MinGW; an MSVC Qt kit needs the matching Visual Studio environment.
- **Application does not start outside Qt Creator**: make Qt runtime DLLs discoverable by adding the kit's `bin` directory to `PATH`, or run `windeployqt` on the built executable.
- **No files found**: check extensions are comma-separated and include the intended text formats.
