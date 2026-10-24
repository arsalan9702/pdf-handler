#pragma once

#include "core/Errors.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace pdfcore {

using DocId = std::uint32_t;

// One output page: page `pageIndex` (0-based) of source `sourceDocId`, rotated clockwise by
// `rotation` degrees (multiple of 90) relative to the source page's own rotation.
struct PageRef {
    DocId sourceDocId = 0;
    int pageIndex = 0;
    int rotation = 0;

    bool operator==(const PageRef&) const = default;
};

struct SourceDoc {
    DocId id = 0;
    std::filesystem::path path;
    std::string password;
};

// The non-destructive edit model: nothing touches disk until a job writes it.
struct Project {
    std::vector<SourceDoc> sources;
    std::vector<PageRef> pages;
};

struct OpResult {
    std::vector<std::string> warnings;
    std::vector<std::filesystem::path> outputs;
};

class CancelToken {
  public:
    void cancel() noexcept { m_cancelled.store(true, std::memory_order_relaxed); }
    [[nodiscard]] bool cancelled() const noexcept {
        return m_cancelled.load(std::memory_order_relaxed);
    }

  private:
    std::atomic<bool> m_cancelled{false};
};

using ProgressFn = std::function<void(int done, int total)>;

// Passed to every job. Both members are optional.
struct JobContext {
    const CancelToken* cancel = nullptr;
    ProgressFn progress;

    void checkCancel() const {
        if (cancel != nullptr && cancel->cancelled()) {
            throw PdfError(ErrorKind::Cancelled, "Operation cancelled");
        }
    }

    void report(int done, int total) const {
        if (progress) {
            progress(done, total);
        }
    }
};

} // namespace pdfcore
