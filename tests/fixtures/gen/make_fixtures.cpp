// Generates every PDF test fixture into the directory given as argv[1].
// Each page carries a visible "P<n>" label and a /PdfhMarker <n> integer (1-based) in its page
// dictionary so tests can verify page order after any operation.

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <qpdf/Pl_DCT.hh>
#include <qpdf/Pl_String.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFWriter.hh>
#include <string>
#include <vector>

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

// Smooth gradient with a little deterministic texture: compresses well with JPEG, poorly-ish
// with Flate, like a real photo.
std::string gradient(int w, int h, int comps) {
    std::string px;
    px.reserve(static_cast<std::size_t>(w * h * comps));
    unsigned seed = 12345;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            seed = seed * 1103515245 + 12345;
            const int noise = static_cast<int>((seed >> 16) % 24);
            for (int c = 0; c < comps; ++c) {
                const int v = (x * 255 / w + y * 128 / h + c * 60 + noise) % 256;
                px.push_back(static_cast<char>(v));
            }
        }
    }
    return px;
}

std::string encodeJpeg(int w, int h, int comps, J_COLOR_SPACE cs, const std::string& pixels) {
    std::string out;
    Pl_String sink("jpeg", nullptr, out);
    Pl_DCT dct("dct", &sink, static_cast<JDIMENSION>(w), static_cast<JDIMENSION>(h), comps, cs);
    dct.write(reinterpret_cast<const unsigned char*>(pixels.data()), pixels.size());
    dct.finish();
    return out;
}

QOH imageStream(QPDF& pdf, int w, int h, const std::string& cs, const std::string& data,
                const std::string& extraDict = "") {
    auto img = pdf.newStream(data);
    auto dict = img.getDict();
    dict.replaceKey("/Type", QOH::newName("/XObject"));
    dict.replaceKey("/Subtype", QOH::newName("/Image"));
    dict.replaceKey("/Width", QOH::newInteger(w));
    dict.replaceKey("/Height", QOH::newInteger(h));
    dict.replaceKey("/BitsPerComponent", QOH::newInteger(8));
    dict.replaceKey("/ColorSpace", QOH::newName(cs));
    if (!extraDict.empty()) {
        auto extra = QOH::parse(extraDict);
        for (const auto& key : extra.getKeys()) {
            dict.replaceKey(key, extra.getKey(key));
        }
    }
    return img;
}

// A page drawing each image in a row; carries a /PdfhMarker like every fixture page.
void addImagePage(QPDF& pdf, int marker, const std::vector<QOH>& images) {
    auto xobjects = QOH::newDictionary();
    std::string content;
    for (std::size_t i = 0; i < images.size(); ++i) {
        const auto name = "/Im" + std::to_string(i);
        xobjects.replaceKey(name, images[i]);
        content += "q 100 0 0 100 " + std::to_string(20 + 110 * i) + " 600 cm " + name + " Do Q\n";
    }
    auto resources = QOH::newDictionary();
    resources.replaceKey("/XObject", xobjects);
    auto page = pdf.makeIndirectObject(QOH::newDictionary());
    page.replaceKey("/Type", QOH::newName("/Page"));
    page.replaceKey("/MediaBox", QOH::parse("[0 0 612 792]"));
    page.replaceKey("/Resources", resources);
    page.replaceKey("/Contents", pdf.newStream(content));
    page.replaceKey("/PdfhMarker", QOH::newInteger(marker));
    QPDFPageDocumentHelper(pdf).addPage(QPDFPageObjectHelper(page), false);
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

        { // images: eligible RGB + gray, and ones the lossy pass must leave alone
            QPDF pdf;
            pdf.emptyPDF();
            auto rgb = imageStream(pdf, 256, 192, "/DeviceRGB", gradient(256, 192, 3));
            auto gray = imageStream(pdf, 160, 160, "/DeviceGray", gradient(160, 160, 1));
            auto cmyk = imageStream(pdf, 96, 96, "/DeviceCMYK", gradient(96, 96, 4));
            auto inverted = imageStream(pdf, 128, 128, "/DeviceGray", gradient(128, 128, 1),
                                        "<< /Decode [1 0] >>");
            auto tiny = imageStream(pdf, 16, 16, "/DeviceRGB", gradient(16, 16, 3));
            auto mask = imageStream(pdf, 128, 96, "/DeviceGray", gradient(128, 96, 1));
            auto masked = imageStream(pdf, 128, 96, "/DeviceRGB", gradient(128, 96, 3));
            masked.getDict().replaceKey("/SMask", mask);
            addImagePage(pdf, 1, {rgb, gray, cmyk});
            addImagePage(pdf, 2, {inverted, tiny, masked, rgb}); // rgb shared across pages
            write(pdf, dir / "images.pdf");
            auto enc = std::make_unique<QPDF>();
            enc->processFile(toUtf8(dir / "images.pdf").c_str());
            write(*enc, dir / "images_encrypted.pdf",
                  [](QPDFWriter& w) { allPermissionsR6(w, "user", "owner"); });
        }

        writeBytes(dir / "photo.jpg", encodeJpeg(320, 240, 3, JCS_RGB, gradient(320, 240, 3)));
        writeBytes(dir / "gray.jpg", encodeJpeg(200, 300, 1, JCS_GRAYSCALE, gradient(200, 300, 1)));
        writeBytes(dir / "cmyk.jpg", encodeJpeg(64, 64, 4, JCS_CMYK, gradient(64, 64, 4)));

        fs::copy_file(dir / "three.pdf", dir / fs::path(u8"ünïcødé 文件.pdf"),
                      fs::copy_options::overwrite_existing);
    } catch (const std::exception& e) {
        std::cerr << "make_fixtures: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
