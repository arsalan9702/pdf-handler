// Drives the real QML interface with synthetic mouse input (offscreen).

#include "TestUtil.hpp"
#include "services/AppController.hpp"
#include "services/IconProvider.hpp"
#include "services/ThumbnailProvider.hpp"

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTest>
#include <QWheelEvent>
#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <memory>

Q_IMPORT_QML_PLUGIN(PdfHandlerPlugin)

using testutil::fixture;

namespace {

// Every warning Qt/QML emits during the tests (binding loops, JS TypeErrors, ...).
QStringList g_warnings;

template <typename F> bool waitFor(F done, int ms = 15000) {
    QElapsedTimer t;
    t.start();
    while (!done() && t.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return done();
}

struct Ui {
    std::unique_ptr<QQmlApplicationEngine> engine;
    QQuickWindow* window = nullptr;
    QObject* project = nullptr;
    QQuickItem* grid = nullptr;

    explicit Ui(const std::string& pdf) {
        engine = std::make_unique<QQmlApplicationEngine>();
        engine->addImageProvider(QStringLiteral("thumb"), new ThumbnailProvider);
        engine->addImageProvider(QStringLiteral("icon"), new IconProvider);
        engine->loadFromModule("PdfHandler", "Main");
        window = qobject_cast<QQuickWindow*>(engine->rootObjects().value(0));
        REQUIRE(window != nullptr);
        window->resize(1240, 800);
        REQUIRE(QTest::qWaitForWindowExposed(window));
        AppController::instance()->openFiles({QString::fromStdU16String(fixture(pdf).u16string())});
        project = window->findChild<QObject*>(QStringLiteral("pageProject"));
        grid = window->findChild<QQuickItem*>(QStringLiteral("pageGrid"));
        REQUIRE(project != nullptr);
        REQUIRE(grid != nullptr);
        REQUIRE(waitFor([&] { return count() > 0 && tile(0) != nullptr; }));
        QTest::qWait(300); // let layout and thumbnails settle
    }

    int count() const { return project->property("count").toInt(); }

    // Marker of the page shown at `row`, i.e. the source page number (1-based).
    int pageNumberAt(int row) const {
        QVariantMap page;
        QMetaObject::invokeMethod(project, "pageAt", Q_RETURN_ARG(QVariantMap, page),
                                  Q_ARG(int, row));
        return page.value(QStringLiteral("pageNumber")).toInt();
    }

    // Delegates are visual children of the grid's contentItem, not QObject children.
    QQuickItem* tile(int row) const {
        auto* content = grid->property("contentItem").value<QQuickItem*>();
        for (auto* item : content->childItems()) {
            if (item->objectName() == QLatin1String("pageTile") &&
                item->property("index").toInt() == row && item->isVisible()) {
                return item;
            }
        }
        return nullptr;
    }

    // Visual descendant named `name` (Repeater/Row children are not QObject children either).
    static QQuickItem* find(QQuickItem* root, const QString& name) {
        for (auto* child : root->childItems()) {
            if (child->objectName() == name) {
                return child;
            }
            if (auto* hit = find(child, name)) {
                return hit;
            }
        }
        return nullptr;
    }

    QPoint center(QQuickItem* item, qreal fx = 0.5, qreal fy = 0.5) const {
        return item->mapToScene(QPointF(item->width() * fx, item->height() * fy)).toPoint();
    }

    void drag(QPoint from, QPoint to) {
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, from);
        const int steps = 20;
        for (int i = 1; i <= steps; ++i) {
            QTest::mouseMove(window, from + (to - from) * i / steps, 10);
        }
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, to);
        QTest::qWait(100);
    }
};

} // namespace

TEST_CASE("Organizer: drag a page to a new position") {
    Ui ui("ten.pdf");
    REQUIRE(ui.count() == 10);
    auto* first = ui.tile(0);
    auto* third = ui.tile(2);
    REQUIRE(first);
    REQUIRE(third);

    // Drop page 1 onto the right half of page 3: it lands after page 3.
    ui.drag(ui.center(first), ui.center(third, 0.8, 0.5));
    CHECK(ui.pageNumberAt(0) == 2);
    CHECK(ui.pageNumberAt(1) == 3);
    CHECK(ui.pageNumberAt(2) == 1);
    CHECK(ui.count() == 10);
    CHECK(ui.project->property("canUndo").toBool());
}

TEST_CASE("Organizer: the delete button on a page removes that page") {
    Ui ui("ten.pdf");
    auto* tile = ui.tile(4);
    REQUIRE(tile);
    QTest::mouseMove(ui.window, ui.center(tile));
    QTest::qWait(100);
    auto* button = Ui::find(tile, QStringLiteral("deleteButton"));
    REQUIRE(button);
    REQUIRE(button->isVisible());
    QTest::mouseClick(ui.window, Qt::LeftButton, Qt::NoModifier, ui.center(button));
    QTest::qWait(100);
    CHECK(ui.count() == 9);
    CHECK(ui.pageNumberAt(4) == 6);
}

TEST_CASE("Organizer: dragging a selected page moves the whole selection") {
    Ui ui("ten.pdf");
    QTest::mouseClick(ui.window, Qt::LeftButton, Qt::NoModifier, ui.center(ui.tile(0), 0.3, 0.7));
    QTest::mouseClick(ui.window, Qt::LeftButton, Qt::ControlModifier,
                      ui.center(ui.tile(1), 0.3, 0.7));
    REQUIRE(ui.project->property("selectedCount").toInt() == 2);
    ui.drag(ui.center(ui.tile(1)), ui.center(ui.tile(4), 0.8, 0.5));
    // Pages 1 and 2 now follow page 5.
    CHECK(ui.pageNumberAt(0) == 3);
    CHECK(ui.pageNumberAt(2) == 5);
    CHECK(ui.pageNumberAt(3) == 1);
    CHECK(ui.pageNumberAt(4) == 2);
    // And undo puts them back.
    QMetaObject::invokeMethod(ui.project, "undo");
    CHECK(ui.pageNumberAt(0) == 1);
}

TEST_CASE("Organizer: rotate button turns one page") {
    Ui ui("ten.pdf");
    auto* tile = ui.tile(3);
    QTest::mouseMove(ui.window, ui.center(tile));
    QTest::qWait(100);
    auto* button = Ui::find(tile, QStringLiteral("rightButton"));
    REQUIRE(button);
    QTest::mouseClick(ui.window, Qt::LeftButton, Qt::NoModifier, ui.center(button));
    QVariantMap page;
    QMetaObject::invokeMethod(ui.project, "pageAt", Q_RETURN_ARG(QVariantMap, page), Q_ARG(int, 3));
    CHECK(page.value(QStringLiteral("pageRotation")).toInt() == 90);
    QMetaObject::invokeMethod(ui.project, "pageAt", Q_RETURN_ARG(QVariantMap, page), Q_ARG(int, 2));
    CHECK(page.value(QStringLiteral("pageRotation")).toInt() == 0);
}

TEST_CASE("Organizer: delete button on a page inside a selection removes only that page") {
    Ui ui("ten.pdf");
    QTest::mouseClick(ui.window, Qt::LeftButton, Qt::NoModifier, ui.center(ui.tile(1), 0.3, 0.7));
    QTest::mouseClick(ui.window, Qt::LeftButton, Qt::ShiftModifier,
                      ui.center(ui.tile(3), 0.3, 0.7));
    REQUIRE(ui.project->property("selectedCount").toInt() == 3);
    auto* tile = ui.tile(2);
    QTest::mouseMove(ui.window, ui.center(tile));
    QTest::qWait(100);
    auto* button = Ui::find(tile, QStringLiteral("deleteButton"));
    REQUIRE(button);
    QTest::mouseClick(ui.window, Qt::LeftButton, Qt::NoModifier, ui.center(button));
    QTest::qWait(100);
    CHECK(ui.count() == 9);
}

TEST_CASE("Organizer: mouse wheel scrolls a long document") {
    Ui ui("images.pdf"); // replaced below by a long document
    ui.window->setProperty("currentKey", QStringLiteral("organize"));
    AppController::instance()->openFiles(
        {QString::fromStdU16String(fixture("ten.pdf").u16string()),
         QString::fromStdU16String(fixture("ten.pdf").u16string()),
         QString::fromStdU16String(fixture("ten.pdf").u16string())});
    REQUIRE(waitFor([&] { return ui.count() == 32; }));
    QTest::qWait(200);

    // Dragging still reorders when the grid can scroll (diagonally, and straight down a row).
    const int first = ui.pageNumberAt(0);
    ui.drag(ui.center(ui.tile(0)), ui.center(ui.tile(6), 0.8, 0.5));
    CHECK(ui.pageNumberAt(6) == first);
    CHECK(ui.grid->property("contentY").toReal() == 0); // the drag did not scroll the grid
    QMetaObject::invokeMethod(ui.project, "undo");

    const qreal before = ui.grid->property("contentY").toReal();
    const QPoint p =
        ui.grid->mapToScene(QPointF(ui.grid->width() / 2, ui.grid->height() / 2)).toPoint();
    QWheelEvent wheel(p, ui.window->mapToGlobal(p), {}, {0, -360}, Qt::NoButton, Qt::NoModifier,
                      Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(ui.window, &wheel);
    QTest::qWait(400);
    CHECK(ui.grid->property("contentY").toReal() > before);
}

TEST_CASE("Every page loads and runs without QML warnings") {
    g_warnings.clear();
    Ui ui("ten.pdf");
    const QStringList pages{"home",       "organize",   "merge",   "split",
                            "compress",   "protect",    "unlock",  "properties",
                            "images2pdf", "pdf2images", "settings"};
    for (const auto& key : pages) {
        ui.window->setProperty("currentKey", key);
        QTest::qWait(150);
    }
    // Interact a little so delegate bindings and handlers run too.
    ui.window->setProperty("currentKey", QStringLiteral("organize"));
    QTest::qWait(150);
    QTest::mouseMove(ui.window, ui.center(ui.tile(0)));
    QTest::mouseClick(ui.window, Qt::LeftButton, Qt::NoModifier, ui.center(ui.tile(0), 0.3, 0.7));
    QTest::mouseDClick(ui.window, Qt::LeftButton, Qt::NoModifier, ui.center(ui.tile(0), 0.3, 0.7));
    QTest::qWait(300);
    INFO(g_warnings.join(QStringLiteral("\n")).toStdString());
    CHECK(g_warnings.isEmpty());
}

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    // Surface QML warnings (binding/JS errors) in the test log.
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext&, const QString& msg) {
        if (type != QtDebugMsg) {
            std::cerr << "[qt] " << msg.toStdString() << "\n";
            g_warnings << msg;
        }
    });
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("PdfHandlerTests"));
    QGuiApplication::setApplicationName(QStringLiteral("ui_tests"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    return Catch::Session().run(argc, argv);
}
