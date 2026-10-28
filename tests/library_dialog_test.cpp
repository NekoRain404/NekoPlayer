#include "library/medialibrary.h"
#include "ui/librarydialog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSortFilterProxyModel>
#include <QTemporaryDir>
#include <QTreeView>
#include <QtTest>

namespace {
bool touch(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly);
}

bool scanAndWait(MediaLibrary &library) {
    QSignalSpy finished(&library, &MediaLibrary::scanFinished);
    library.startScan();
    return finished.wait(10000);
}

struct DialogFixture {
    QTemporaryDir temporary;
    QString path;
    MediaLibrary library;
    LibraryDialog dialog;

    DialogFixture()
        : path(temporary.filePath(QStringLiteral("library.ini"))), library(path), dialog(library) {
        QDir(temporary.path()).mkdir(QStringLiteral("media"));
        const auto media = temporary.filePath(QStringLiteral("media"));
        touch(QDir(media).filePath(QStringLiteral("Episode 2.mkv")));
        touch(QDir(media).filePath(QStringLiteral("Movie.mp4")));
        touch(QDir(media).filePath(QStringLiteral("Song.flac")));
        (void)library.addFolder(media);
        (void)scanAndWait(library);
        (void)library.save();
        dialog.show();
        QCoreApplication::processEvents();
    }

    [[nodiscard]] QLineEdit *search() const {
        return dialog.findChild<QLineEdit *>(QStringLiteral("librarySearchEdit"));
    }
    [[nodiscard]] QTreeView *view() const {
        return dialog.findChild<QTreeView *>(QStringLiteral("libraryMediaView"));
    }
    [[nodiscard]] QSortFilterProxyModel *model() const {
        return qobject_cast<QSortFilterProxyModel *>(view()->model());
    }
};
} // namespace

class LibraryDialogTest final : public QObject {
    Q_OBJECT
  private:
    QVariant previousConfiguration_;

  private slots:
    void init() {
        previousConfiguration_ = qApp->property("configurationEnabled");
        qApp->setProperty("configurationEnabled", true);
    }

    void cleanup() {
        if (previousConfiguration_.isValid())
            qApp->setProperty("configurationEnabled", previousConfiguration_);
        else
            qApp->setProperty("configurationEnabled", QVariant{});
    }

    void filtersByDisplayNamePathAndShowsEmptyState() {
        DialogFixture fixture;
        auto *search = fixture.search();
        auto *model = fixture.model();
        QVERIFY(search);
        QVERIFY(model);
        QCOMPARE(search->placeholderText(), QStringLiteral("搜索媒体库"));
        QCOMPARE(model->rowCount(), 3);

        search->setText(QStringLiteral("movie"));
        QCOMPARE(model->rowCount(), 1);
        search->setText(QStringLiteral("MOVIE"));
        QCOMPARE(model->rowCount(), 1);
        search->setText(QStringLiteral("Episode 2"));
        QCOMPARE(model->rowCount(), 1);
        search->setText(QStringLiteral("media"));
        QCOMPARE(model->rowCount(), 3);

        search->setText(QStringLiteral("no-match"));
        QCOMPARE(model->rowCount(), 0);
        auto *empty = fixture.dialog.findChild<QLabel *>(QStringLiteral("libraryEmptyHint"));
        QVERIFY(empty);
        QVERIFY(empty->isVisible());
        QCOMPARE(empty->text(), QStringLiteral("没有匹配的媒体。"));

        search->clear();
        QCOMPARE(model->rowCount(), 3);
        QVERIFY(!empty->isVisible());
    }

    void emitsPlayAndAppendForSelection() {
        DialogFixture fixture;
        auto *view = fixture.view();
        auto *model = fixture.model();
        QVERIFY(view);
        QVERIFY(model);
        const auto index = model->index(0, 0);
        QVERIFY(index.isValid());
        view->setCurrentIndex(index);
        view->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect |
                                                  QItemSelectionModel::Rows);
        QCoreApplication::processEvents();

        auto *play = fixture.dialog.findChild<QPushButton *>(QStringLiteral("libraryPlayButton"));
        auto *append =
            fixture.dialog.findChild<QPushButton *>(QStringLiteral("libraryAppendButton"));
        QVERIFY(play);
        QVERIFY(append);
        QVERIFY(play->isEnabled());
        QVERIFY(append->isEnabled());

        QSignalSpy plays(&fixture.dialog, &LibraryDialog::playRequested);
        play->click();
        QCOMPARE(plays.count(), 1);
        QCOMPARE(plays.at(0).at(0).toString(),
                 view->model()->index(0, 0).data(Qt::UserRole).toString());

        view->selectAll();
        QCoreApplication::processEvents();
        QSignalSpy appends(&fixture.dialog, &LibraryDialog::appendRequested);
        append->click();
        QCOMPARE(appends.count(), 1);
        QCOMPARE(appends.at(0).at(0).toStringList().size(), 3);
    }

    void memoryOnlyHintAppearsWhenConfigurationIsDisabled() {
        qApp->setProperty("configurationEnabled", false);
        DialogFixture fixture;
        auto *hint = fixture.dialog.findChild<QLabel *>(QStringLiteral("libraryMemoryOnlyHint"));
        QVERIFY(hint);
        QVERIFY(hint->isVisible());
    }
};

QTEST_MAIN(LibraryDialogTest)
#include "library_dialog_test.moc"
