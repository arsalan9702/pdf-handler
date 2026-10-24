#include "core/Version.hpp"

#include <qpdf/QPDF.hh>

namespace pdfcore {

std::string qpdfVersion() {
    return QPDF::QPDFVersion();
}

} // namespace pdfcore
