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

// A top-level outline entry pointing at a 0-based page of the output.
struct Bookmark {
    std::string title;
    int outputPageIndex = 0;
};

// The non-destructive edit model: nothing touches disk until a job writes it.
struct Project {
    std::vector<SourceDoc> sources;
    std::vector<PageRef> pages;
    // When non-empty, replaces the output's outline with these entries.
    std::vector<Bookmark> bookmarks;
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

    // A child context whose progress (done/childTotal) maps onto [offset, offset + span] of
    // `total` units of this context. The child must not outlive this context.
    [[nodiscard]] JobContext slice(int offset, int span, int total) const {
        JobContext child;
        child.cancel = cancel;
        if (progress) {
            child.progress = [this, offset, span, total](int done, int childTotal) {
                const int mapped = childTotal > 0 ? offset + span * done / childTotal : offset;
                report(mapped, total);
            };
        }
        return child;
    }
};

} // namespace pdfcore
