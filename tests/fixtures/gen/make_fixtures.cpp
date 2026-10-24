// Generates every PDF test fixture into the directory given as argv[1].
// Each page carries a visible "P<n>" label and a /PdfhMarker <n> integer (1-based) in its page
// dictionary so tests can verify page order after any operation.

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFWriter.hh>
#include <string>

namespace fs = std::filesystem;
using QOH = QPDFObjectHandle;

namespace {

struct PageSpec {
    double width = 612;
    double height = 792;
    int rotate = 0;
};

std::string toUtf8(const fs::path& p) {
    auto u8 = p.u8string();
    return {u8.begin(), u8.end()};
}

QOH helvetica(QPDF& pdf) {
    return pdf.makeIndirectObject(
        QOH::parse("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>"));
}

void addMarkedPage(QPDF& pdf, QOH font, int marker, const PageSpec& spec = {}) {
    const std::string label = "P" + std::to_string(marker);
    auto contents = pdf.newStream("BT /F1 48 Tf 72 " + std::to_string(int(spec.height) - 120) +
                                  " Td (" + label + ") Tj ET");
    auto page = pdf.makeIndirectObject(QOH::newDictionary());
    page.replaceKey("/Type", QOH::newName("/Page"));
    page.replaceKey("/MediaBox", QOH::parse("[0 0 " + std::to_string(spec.width) + " " +
                                            std::to_string(spec.height) + "]"));
    auto fonts = QOH::newDictionary();
    fonts.replaceKey("/F1", font);
    auto resources = QOH::newDictionary();
    resources.replaceKey("/Font", fonts);
    page.replaceKey("/Resources", resources);
    page.replaceKey("/Contents", contents);
    page.replaceKey("/PdfhMarker", QOH::newInteger(marker));
    if (spec.rotate != 0) {
        page.replaceKey("/Rotate", QOH::newInteger(spec.rotate));
    }
    QPDFPageDocumentHelper(pdf).addPage(QPDFPageObjectHelper(page), false);
}

std::unique_ptr<QPDF> markedDoc(int pages) {
    auto pdf = std::make_unique<QPDF>();
    pdf->emptyPDF();
    auto font = helvetica(*pdf);
    for (int i = 1; i <= pages; ++i) {
        addMarkedPage(*pdf, font, i);
    }
    return pdf;
}

void write(QPDF& pdf, const fs::path& path,
           const std::function<void(QPDFWriter&)>& configure = {}) {
    const auto name = toUtf8(path);
    QPDFWriter w(pdf, name.c_str());
    w.setStaticID(true);
    w.setObjectStreamMode(qpdf_o_disable);
    if (configure) {
        configure(w);
    }
    w.write();
}

std::string readAll(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

void writeBytes(const fs::path& path, const std::string& data) {
    std::ofstream(path, std::ios::binary) << data;
}

void allPermissionsR6(QPDFWriter& w, const char* user, const char* owner) {
    w.setStaticAesIV(true);
    w.setR6EncryptionParameters(user, owner, true, true, true, true, true, true, qpdf_r3p_full,
                                true);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: make_fixtures <output-dir>\n";
        return 2;
    }
    const fs::path dir = fs::path(reinterpret_cast<const char8_t*>(argv[1]));
    fs::create_directories(dir);

    try {
        write(*markedDoc(1), dir / "single.pdf");
        write(*markedDoc(10), dir / "ten.pdf");
        write(*markedDoc(3), dir / "three.pdf");

        { // mixed page sizes and rotations, including an inherited /Rotate
            QPDF pdf;
            pdf.emptyPDF();
            auto font = helvetica(pdf);
            addMarkedPage(pdf, font, 1);
            addMarkedPage(pdf, font, 2, {842, 595, 0});
            addMarkedPage(pdf, font, 3, {612, 792, 90});
            addMarkedPage(pdf, font, 4, {420, 595, 270});
            // Page 1 has no /Rotate of its own and inherits 180 from the page tree root.
            pdf.getRoot().getKey("/Pages").replaceKey("/Rotate", QOH::newInteger(180));
            write(pdf, dir / "mixed.pdf");
        }

        { // encrypted, user password required
            auto pdf = markedDoc(5);
            write(*pdf, dir / "encrypted_user.pdf",
                  [](QPDFWriter& w) { allPermissionsR6(w, "user", "owner"); });
        }
        { // encrypted, opens without a password (owner password only)
            auto pdf = markedDoc(5);
            write(*pdf, dir / "encrypted_owner_only.pdf",
                  [](QPDFWriter& w) { allPermissionsR6(w, "", "owner"); });
        }
        { // legacy RC4 128-bit
            auto pdf = markedDoc(4);
            write(*pdf, dir / "encrypted_rc4.pdf", [](QPDFWriter& w) {
                w.setR3EncryptionParametersInsecure("user", "owner", true, true, true, true, true,
                                                    true, qpdf_r3p_full);
            });
        }

        { // link annotation, text form field, and an outline entry pointing at page 3
            auto pdf = markedDoc(4);
            auto pages = pdf->getAllPages();
            auto link = pdf->makeIndirectObject(
                QOH::parse("<< /Type /Annot /Subtype /Link /Rect [72 500 300 530] /Border [0 0 1]"
                           " /A << /S /URI /URI (https://example.com) >> >>"));
            auto field = pdf->makeIndirectObject(
                QOH::parse("<< /Type /Annot /Subtype /Widget /FT /Tx /T (name) /V (hello)"
                           " /Rect [72 400 300 430] /DA (/Helv 12 Tf 0 g) /F 4 >>"));
            field.replaceKey("/P", pages[0]);
            auto annots = QOH::newArray();
            annots.appendItem(link);
            annots.appendItem(field);
            pages[0].replaceKey("/Annots", annots);

            auto acroForm = QOH::parse("<< /DA (/Helv 12 Tf 0 g) >>");
            auto fields = QOH::newArray();
            fields.appendItem(field);
            acroForm.replaceKey("/Fields", fields);
            auto dr = QOH::newDictionary();
            auto drFonts = QOH::newDictionary();
            drFonts.replaceKey("/Helv", helvetica(*pdf));
            dr.replaceKey("/Font", drFonts);
            acroForm.replaceKey("/DR", dr);
            pdf->getRoot().replaceKey("/AcroForm", pdf->makeIndirectObject(acroForm));

            auto outlines = pdf->makeIndirectObject(QOH::parse("<< /Type /Outlines /Count 1 >>"));
            auto item = pdf->makeIndirectObject(QOH::parse("<< /Title (Chapter 3) >>"));
            item.replaceKey("/Parent", outlines);
            auto dest = QOH::newArray();
            dest.appendItem(pages[2]);
            dest.appendItem(QOH::newName("/Fit"));
            item.replaceKey("/Dest", dest);
            outlines.replaceKey("/First", item);
            outlines.replaceKey("/Last", item);
            pdf->getRoot().replaceKey("/Outlines", outlines);
            write(*pdf, dir / "interactive.pdf");
        }

        write(*markedDoc(6), dir / "objstreams.pdf",
              [](QPDFWriter& w) { w.setObjectStreamMode(qpdf_o_generate); });
        write(*markedDoc(6), dir / "linearized.pdf",
              [](QPDFWriter& w) { w.setLinearization(true); });

        { // damaged: wrong startxref offset forces xref reconstruction
            auto data = readAll(dir / "ten.pdf");
            const auto pos = data.rfind("startxref");
            data.replace(pos, std::string::npos, "startxref\n123\n%%EOF\n");
            writeBytes(dir / "broken_xref.pdf", data);
        }
        { // truncated: last 40% of the file is gone, including xref and trailer
            auto data = readAll(dir / "ten.pdf");
            writeBytes(dir / "truncated.pdf", data.substr(0, data.size() * 6 / 10));
        }
        writeBytes(dir / "not_a_pdf.pdf", "This is just a text file pretending to be a PDF.\n");

        fs::copy_file(dir / "three.pdf", dir / fs::path(u8"ünïcødé 文件.pdf"),
                      fs::copy_options::overwrite_existing);
    } catch (const std::exception& e) {
        std::cerr << "make_fixtures: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
