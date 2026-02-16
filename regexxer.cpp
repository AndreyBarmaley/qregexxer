/***************************************************************************
 *   Copyright © 2026 by Andrey Afletdinov <public.irkutsk@gmail.com>      *
 *                                                                         *
 *   qregexxer: search/replace tool for the desktop users                  *
 *   https://github.com/AndreyBarmaley/qregexxer                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "help.h"
#include "regexxer.h"

#include <QDir>
#include <QFile>
#include <QPair>
#include <QFont>
#include <QFile>
#include <QMenu>
#include <QIcon>
#include <QBrush>
#include <QLabel>
#include <QDebug>
#include <QAction>
#include <QPalette>
#include <QSaveFile>
#include <QComboBox>
#include <QCheckBox>
#include <QSettings>
#include <QFileInfo>
#include <QKeyEvent>
#include <QTextEdit>
#include <QLineEdit>
#include <QCloseEvent>
#include <QMessageBox>
#include <QTextCursor>
#include <QTextStream>
#include <QFileDialog>
#include <QStringList>
#include <QFontDatabase>
#include <QTextDocument>
#include <QTreeWidgetItem>
#include <QCoreApplication>
#include <QCommandLineParser>

#include <algorithm>

namespace {

    class MultiWordHighlighter : public QSyntaxHighlighter {

        const QRegularExpression & pattern_;
        QTextCharFormat format_;

      protected:
        void highlightBlock(const QString & text) override {
            auto matchIterator = pattern_.globalMatch(text);

            while(matchIterator.hasNext()) {
                QRegularExpressionMatch match = matchIterator.next();
                setFormat(match.capturedStart(), match.capturedLength(), format_);
            }
        }

      public:
        MultiWordHighlighter(QTextEdit* text_edit, const QColor & match_color, const QRegularExpression & regex)
            : QSyntaxHighlighter(text_edit->document()), pattern_(regex) {
            format_.setForeground(text_edit->textColor());
            format_.setBackground(match_color);
        }
    };

    struct StringData : QPair<QString, bool> {
        StringData(const QString & str, bool save) : QPair<QString, bool> {
            str, save
        } {}
        StringData() {}

        QString string(void) const {
            return first;
        }

        bool save(void) const {
            return second;
        }
    };

    struct ListData : QPair<QStringList, bool> {
        ListData(const QStringList & list, bool save) : QPair<QStringList, bool> {
            list, save
        } {}
        ListData(QStringList && list, bool save) : QPair<QStringList, bool> {
            std::forward<QStringList>(list), save
        } {}
        ListData() {}

        QString string(void) const {
            return first.join(" ");
        }

        const QStringList & list(void) const {
            return first;
        }

        bool save(void) const {
            return second;
        }
    };

    QList<OffsetLength> searchPositions(const QString & content, const QRegularExpression & regex) {

        QList<OffsetLength> res;

        if(! content.isEmpty() && regex.isValid()) {
            auto it = regex.globalMatch(content);

            while(it.hasNext()) {
                auto match = it.next();

                int offset = match.capturedStart();
                int length = match.capturedLength();
                res << QPair<int, int>(offset, length);
            }
        }

        return res;
    }
}

Q_DECLARE_METATYPE(StringData);
Q_DECLARE_METATYPE(ListData);

bool FileData::isValid(void) const {
    return ! is_error_;
}

bool FileData::isChanges(void) const {
    return document_.isModified();
}

int FileData::matches(void) const {
    return search_.size();
}

bool FileData::isCursorPrev(void) const {
    return 0 < curpos_;
}

bool FileData::isCursorNext(void) const {
    return curpos_ + 1 < search_.size();
}

bool FileData::isCursorLast(void) const {
    return curpos_ + 1 == search_.size();
}

const OffsetLength* FileData::cursorPosition(void) const {
    return curpos_ < search_.size() ? std::addressof(search_[curpos_]) : nullptr;
}

void FileData::clearPositions(void) {
    curpos_ = 0;
    search_.clear();
}

void FileData::updatePositions(const QRegularExpression & regex) {
    curpos_ = 0;
    search_ = searchPositions(document_.toPlainText(), regex);
}

FilesMatchesResult & FilesMatchesResult::operator+=(const FilesMatchesResult & res) {
    first += res.first;
    second += res.second;
    return *this;
}

MainRegexxer::MainRegexxer(const QCommandLineParser & parser)
    : settings_(QSettings::IniFormat, QSettings::UserScope, "qregexxer", "settings"),
      last_pattern_("*"), ui_(new Ui::regexxer) {
    ui_->setupUi(this);

    ui_->cbSearch_->setDisabled(true);
    ui_->cbReplace_->setDisabled(true);

    ui_->cbRecursive_->setCheckState(Qt::Checked);
    ui_->cbHidden_->setCheckState(Qt::Unchecked);
    ui_->cbGlobal_->setCheckState(Qt::Checked);
    ui_->cbNoCase_->setCheckState(Qt::Unchecked);

    ui_->cbGlobal_->setToolTip(tr("find global match"));
    ui_->cbNoCase_->setToolTip(tr("do case insensitive matching"));

    auto boolSettings = [](const QString & name, const QSettings & settings, const QCommandLineParser & parser) {
        if(parser.isSet(name)) {
            return true;
        }

        return qvariant_cast<bool>(settings.value(name));
    };

    ui_->cbRecursive_->setCheckState(boolSettings("no-recursion", settings_, parser) ? Qt::Unchecked : Qt::Checked);
    ui_->cbHidden_->setCheckState(boolSettings("hidden", settings_, parser) ? Qt::Checked : Qt::Unchecked);
    ui_->cbNoCase_->setCheckState(boolSettings("ignore-case", settings_, parser) ? Qt::Checked : Qt::Unchecked);
    ui_->cbGlobal_->setCheckState(boolSettings("no-global", settings_, parser) ? Qt::Unchecked : Qt::Checked);
    ui_->cbGlobal_->setVisible(false);

    // init abuttons
    ui_->btFindFiles_->setEnabled(true);
    ui_->btFind_->setDisabled(true);
    ui_->btReplace_->setDisabled(true);
    ui_->btThisFile_->setDisabled(true);
    ui_->btAllFiles_->setDisabled(true);
    ui_->btPrevMatch_->setDisabled(true);
    ui_->btNextMatch_->setDisabled(true);
    ui_->btPreviousFile_->setDisabled(true);
    ui_->btNextFile_->setDisabled(true);

    ui_->btFindFiles_->setToolTip(tr("find files"));
    ui_->btFind_->setToolTip(tr("find matches"));
    ui_->btReplace_->setToolTip(tr("replace one match"));
    ui_->btThisFile_->setToolTip(tr("replace all matches in this file"));
    ui_->btAllFiles_->setToolTip(tr("replace all matches in all files"));
    ui_->btPrevMatch_->setToolTip(tr("back match"));
    ui_->btNextMatch_->setToolTip(tr("forward match"));
    ui_->btPreviousFile_->setToolTip(tr("previous file"));
    ui_->btNextFile_->setToolTip(tr("next file"));
    ui_->btHelpRegex_->setToolTip(tr("help QRegex"));

    ui_->btPrevMatch_->setText("");
    ui_->btNextMatch_->setText("");
    ui_->btPreviousFile_->setText("");
    ui_->btNextFile_->setText("");
    ui_->btHelpRegex_->setText("");

    // init actions
    ui_->actionOpenDir_->setEnabled(true);
    ui_->actionSaveCurrent_->setDisabled(true);
    ui_->actionSaveAll_->setDisabled(true);
    ui_->actionQuit_->setEnabled(true);

    // init folders
    loadCbFolders(*ui_->cbFolders_,
                  qvariant_cast<QStringList>(settings_.value("folders")) << parser.positionalArguments());

    // init patterns
    loadCbPatterns(*ui_->cbPatterns_,
                   QStringList(parser.value("pattern")) << qvariant_cast<QStringList>(settings_.value("patterns")));

    // init regexps
    ui_->cbSearch_->addItems(QStringList(parser.value("regex")) <<
                             qvariant_cast<QStringList>(settings_.value("search")));

    // init substitutions
    ui_->cbReplace_->addItems(QStringList(parser.value("substitution")) <<
                              qvariant_cast<QStringList>(settings_.value("replace")));

    // init tree widget
    ui_->twFiles_->setColumnCount(2);
    ui_->twFiles_->setHeaderLabels(QStringList() << "File" << "#");

    if(auto header = ui_->twFiles_->header()) {
        header->setStretchLastSection(false);
        header->setSectionResizeMode(0, QHeaderView::Stretch);
        header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    }

    if(! settings_.contains("history_count")) {
        settings_.setValue("history_count", 4);
    }

    // set default font
    if(settings_.contains("default_font")) {
        default_font_.fromString(qvariant_cast<QString>(settings_.value("default_font")));
    } else {
        default_font_ = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        default_font_.setPointSize(10);
    }

    // set default colors
    if(settings_.contains("match_color")) {
        match_color_ = QColor(qvariant_cast<QString>(settings_.value("match_color")));
    } else {
        // light blue
        match_color_ = QColor(0xff, 0xda, 0xb9);
    }

    if(settings_.contains("current_color")) {
        current_color_ = QColor(qvariant_cast<QString>(settings_.value("current_color")));
    } else {
        // peach puff
        current_color_ = QColor(0xad, 0xd8, 0xe6);
    }

    // init icons theme
    if(settings_.contains("icons_theme")) {
        QIcon::setFallbackThemeName(qvariant_cast<QString>(settings_.value("icons_theme")));
    } else {
        QIcon::setFallbackThemeName("oxygen");
    }

    // init text edit
    ui_->teContent_->setReadOnly(true);
    ui_->teContent_->setFont(default_font_);
    auto cursor = ui_->teContent_->textCursor();
    cursor.select(QTextCursor::BlockUnderCursor);
    ui_->teContent_->setTextCursor(cursor);

    ui_->leReplace_->setReadOnly(true);
    ui_->leReplace_->setVisible(false);

    auto palette = ui_->leReplace_->palette();
    palette.setColor(QPalette::Base, palette.color(QPalette::Window));
    ui_->leReplace_->setPalette(palette);

    // init statusbar
    if(auto sbar = statusBar()) {
        status_match_ = new QLabel();
        sbar->addPermanentWidget(status_match_);
        status_files_ = new QLabel();
        sbar->addPermanentWidget(status_files_);
        sbar->setLayoutDirection(Qt::LayoutDirection::RightToLeft);
        updateStatus();
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(ui_->btHelpRegex_, &QPushButton::clicked, this, &MainRegexxer::btHelpRegexClicked);
    connect(ui_->btFindFiles_, &QPushButton::clicked, this, &MainRegexxer::btFindFilesClicked);
    connect(ui_->btFind_, &QPushButton::clicked, this, &MainRegexxer::btFindRegexClicked);
    connect(ui_->actionOpenDir_, &QAction::triggered, this, &MainRegexxer::dialogOpenDirectory);
    connect(ui_->actionQuit_, &QAction::triggered, this, &MainRegexxer::close);
    connect(ui_->actionSaveCurrent_, &QAction::triggered, this, &MainRegexxer::acSaveCurrentClicked);
    connect(ui_->actionSaveAll_, &QAction::triggered, this, &MainRegexxer::acSaveAllClicked);
    connect(ui_->cbFolders_, &QComboBox::activated, this, &MainRegexxer::cbFoldersActivated);
    connect(ui_->cbPatterns_, &QComboBox::currentIndexChanged, this, &MainRegexxer::cbPatternsCurrentIndexChanged);
    connect(ui_->cbPatterns_, &QComboBox::editTextChanged, this, &MainRegexxer::cbPatternsEditTextChanged);
    connect(ui_->cbSearch_, &QComboBox::editTextChanged, this, &MainRegexxer::cbSearchEditTextChanged);
    connect(ui_->cbReplace_, &QComboBox::editTextChanged, this, &MainRegexxer::cbReplaceEditTextChanged);
    connect(ui_->twFiles_, &QTreeWidget::itemClicked, this, &MainRegexxer::twFilesItemClicked);
    connect(ui_->twFiles_, &QTreeWidget::itemChanged, this, &MainRegexxer::twFilesItemChanged);
    connect(ui_->cbSearch_->lineEdit(), &QLineEdit::returnPressed, this, &MainRegexxer::cbSearchReturnPressed);
    connect(ui_->cbReplace_->lineEdit(), &QLineEdit::returnPressed, this, &MainRegexxer::cbReplaceReturnPressed);
    connect(ui_->btPreviousFile_, &QPushButton::clicked, this, &MainRegexxer::btPreviousFileClicked);
    connect(ui_->btNextFile_, &QPushButton::clicked, this, &MainRegexxer::btNextFileClicked);
    connect(ui_->btPrevMatch_, &QPushButton::clicked, this, &MainRegexxer::btPrevMatchClicked);
    connect(ui_->btNextMatch_, &QPushButton::clicked, this, &MainRegexxer::btNextMatchClicked);
    connect(ui_->btReplace_, &QPushButton::clicked, this, &MainRegexxer::btReplaceClicked);
    connect(ui_->btThisFile_, &QPushButton::clicked, this, &MainRegexxer::btThisFileClicked);
    connect(ui_->btAllFiles_, &QPushButton::clicked, this, &MainRegexxer::btAllFilesClicked);
#else
    connect(ui_->btHelpRegex_, SIGNAL(clicked()), this, SLOT(btHelpRegexClicked()));
    connect(ui_->btFindFiles_, SIGNAL(clicked()), this, SLOT(btFindFilesClicked()));
    connect(ui_->btFind_, SIGNAL(clicked()), this, SLOT(btFindRegexClicked()));
    connect(ui_->actionOpenDir_, SIGNAL(triggered()), this, SLOT(dialogOpenDirectory()));
    connect(ui_->actionQuit_, SIGNAL(triggered()), this, SLOT(close()));
    connect(ui_->actionSaveCurrent_, SIGNAL(triggered()), this, SLOT(acSaveCurrentClicked()));
    connect(ui_->actionSaveAll_, SIGNAL(triggered()), this, SLOT(acSaveAllClicked()));
    connect(ui_->cbFolders_, SIGNAL(activated(int)), this, SLOT(cbFoldersActivated(int)));
    connect(ui_->cbPatterns_, SIGNAL(currentIndexChanged(int)), this, SLOT(cbPatternsCurrentIndexChanged(int)));
    connect(ui_->cbPatterns_, SIGNAL(editTextChanged(const QString &)), this, SLOT(cbPatternsEditTextChanged(const QString &)));
    connect(ui_->cbSearch_, SIGNAL(editTextChanged(const QString &)), this, SLOT(cbSearchEditTextChanged(const QString &)));
    connect(ui_->cbReplace_, SIGNAL(editTextChanged(const QString &)), this, SLOT(cbReplaceEditTextChanged(const QString &)));
    connect(ui_->twFiles_, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(twFilesItemClicked(QTreeWidgetItem*, int)));
    connect(ui_->twFiles_, SIGNAL(itemChanged(QTreeWidgetItem*, int)), this, SLOT(twFilesItemChanged(QTreeWidgetItem*, int)));
    connect(ui_->cbSearch_->lineEdit(), SIGNAL(returnPressed()), this, SLOT(cbSearchReturnPressed()));
    connect(ui_->cbReplace_->lineEdit(), SIGNAL(returnPressed()), this, SLOT(cbReplaceReturnPressed()));
    connect(ui_->btPreviousFile_, SIGNAL(clicked()), this, SLOT(btPreviousFileClicked()));
    connect(ui_->btNextFile_, SIGNAL(clicked()), this, SLOT(btNextFileClicked()));
    connect(ui_->btPrevMatch_, SIGNAL(clicked()), this, SLOT(btPrevMatchClicked()));
    connect(ui_->btNextMatch_, SIGNAL(clicked()), this, SLOT(btNextMatchClicked()));
    connect(ui_->btReplace_, SIGNAL(clicked()), this, SLOT(btReplaceClicked()));
    connect(ui_->btThisFile_, SIGNAL(clicked()), this, SLOT(btThisFileClicked()));
    connect(ui_->btAllFiles_, SIGNAL(clicked()), this, SLOT(btAllFilesClicked()));
#endif

    if(settings_.contains("window_geometry")) {
        setGeometry(qvariant_cast<QRect>(settings_.value("window_geometry")));
    }

    if(settings_.contains("panel_splitter")) {
        ui_->splitter_->restoreState(qvariant_cast<QByteArray>(settings_.value("panel_splitter")));
    }
}

void MainRegexxer::loadCbFolders(QComboBox & box, QStringList dirs) const {
    dirs.removeDuplicates();

    for(auto & path : dirs) {
        if(QFileInfo(path).isDir()) {
            const QDir dir(path);
            box.addItem(QIcon::fromTheme("inode-directory"), dir.dirName(),
                        QVariant::fromValue(StringData(dir.absolutePath(), true)));
        }
    }

    if(0 == box.count()) {
        const QDir currentDir(QCoreApplication::applicationDirPath());
        box.addItem(QIcon::fromTheme("inode-directory"), currentDir.dirName(),
                    QVariant::fromValue(StringData(currentDir.absolutePath(), false)));
    }

    box.insertSeparator(box.count());
    box.addItem(QIcon::fromTheme("folder-new"), tr("... Open Directory ..."), QVariant::fromValue(StringData()));
}

void MainRegexxer::loadCbPatterns(QComboBox & box, QStringList values) const {
    values.removeDuplicates();

    for(auto & str : values) {
        if(! str.isEmpty()) {
            box.addItem(str, QVariant::fromValue(ListData(str.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts), true)));
        }
    }

    if(box.count()) {
        box.insertSeparator(box.count());
    }

    // defaults patterns
    const QStringList cpp({"*.cpp", "*.cxx", "*.h", "*.hpp", "*.c", "*.cc"});
    box.addItem(cpp.join(" "), QVariant::fromValue(ListData(cpp, false)));

    box.addItem(last_pattern_, QVariant::fromValue(ListData(QStringList(last_pattern_), false)));
}

QList<QTreeWidgetItem*> MainRegexxer::makeFilesItems(const QDir & dir, bool recursive, bool hidden, const QStringList & filters) const {
    QDir::Filters flags = QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Readable | QDir::Writable;

    if(hidden) {
        flags |= QDir::Hidden;
    }

    QFileInfoList files = dir.entryInfoList(filters, flags);
    QList<QTreeWidgetItem*> items;

    for(const auto & info : files) {
        if(info.isDir() && recursive) {
            auto item = new QTreeWidgetItem();
            item->setIcon(0, QIcon::fromTheme("inode-directory"));
            item->setText(0, info.baseName());
            item->setData(0, Qt::UserRole, info.absoluteFilePath());
            item->setText(1, "0");
            item->setData(1, Qt::UserRole, 0);
            item->addChildren(makeFilesItems(info.absoluteFilePath(), recursive, hidden, filters));
            items << item;
        }
    }

    for(const auto & info : files) {
        if(info.isFile()) {
            if(info.isWritable()) {
                auto item = new QTreeWidgetItem();
                item->setIcon(0, QIcon::fromTheme("text-x-generic"));
                item->setText(0, info.fileName());
                item->setData(0, Qt::UserRole, info.absoluteFilePath());
                item->setText(1, "0");
                item->setData(1, Qt::UserRole, 0);
                items << item;
            } else {
                qWarning() << "cannot writeable file:" << info.absoluteFilePath();
            }
        }
    }

    return items;
}

template <typename T>
QStringList cbToSaveList(const QComboBox* box, int history_count) {
    QStringList res;

    for(int it = 0; it < box->count(); ++it) {
        if(auto val = qvariant_cast<T>(box->itemData(it)); val.save()) {
            res << val.string();
        }
    }

    if(history_count < res.size()) {
        res.erase(res.begin() + history_count, res.end());
    }

    return res;
}

QStringList cbToList(const QComboBox* box, int history_count) {
    QStringList res;

    for(int it = 0; it < box->count(); ++it) {
        res << box->itemText(it);
    }

    if(history_count < res.size()) {
        res.erase(res.begin() + history_count, res.end());
    }

    return res;
}

void MainRegexxer::closeEvent(QCloseEvent* ev) {
    bool any_changes = std::any_of(files_.begin(), files_.end(), [](auto & ptr) {
        return ptr->isChanges();
    });

    if(any_changes) {
        if(QMessageBox::No == QMessageBox::warning(this, "Sorry",
                tr("Some files haven't been saved yet.\nContinue anyway?"), QMessageBox::Yes | QMessageBox::No)) {
            ev->ignore();
            return;
        }
    }

    const int history_count = qvariant_cast<int>(settings_.value("history_count"));

    settings_.setValue("no-recursion", Qt::Unchecked == ui_->cbRecursive_->checkState());
    settings_.setValue("hidden", Qt::Checked == ui_->cbHidden_->checkState());
    settings_.setValue("no-global", Qt::Unchecked == ui_->cbGlobal_->checkState());
    settings_.setValue("ignore-case", Qt::Checked == ui_->cbNoCase_->checkState());

    settings_.setValue("window_geometry", geometry());
    settings_.setValue("panel_splitter", ui_->splitter_->saveState());
    settings_.setValue("default_font", default_font_.toString());
    settings_.setValue("icons_theme", QIcon::fallbackThemeName());
    settings_.setValue("match_color", match_color_.name());
    settings_.setValue("current_color", current_color_.name());

    /*
        feedback_ = boolSettings("line-number", settings, parser);
        no_autorun_ = boolSettings("no-autorun", settings, parser);

        settings_.setValue("line-number", feedback_);
        settings_.setValue("no-autorun", no_autorun_);
    */

    // save comboboxes
    settings_.setValue("patterns", cbToSaveList<ListData>(ui_->cbPatterns_, history_count));
    settings_.setValue("folders", cbToSaveList<StringData>(ui_->cbFolders_, history_count));
    settings_.setValue("search", cbToList(ui_->cbSearch_, history_count));
    settings_.setValue("replace", cbToList(ui_->cbReplace_, history_count));

    ev->accept();
}

void MainRegexxer::keyPressEvent(QKeyEvent* ev) {
    if(ev->key() == Qt::Key_Escape) {
        close();
    }
}

void MainRegexxer::dialogOpenDirectory(void) {
    auto & box = ui_->cbFolders_;

    auto data = qvariant_cast<StringData>(box->itemData(0));
    auto path = QFileDialog::getExistingDirectory(this, tr("Open Directory"), data.string(),
                QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if(! path.isEmpty()) {
        const QDir dir(path);
        box->insertItem(0, dir.dirName(), QVariant::fromValue(StringData(dir.absolutePath(), true)));

        box->setCurrentIndex(0);
        // reload twFiles
        btFindFilesClicked();
    }
}

void MainRegexxer::cbFoldersActivated(int index) {
    auto & box = ui_->cbFolders_;

    // last element open dialog
    if(index + 1 == box->count()) {
        dialogOpenDirectory();
    }
}

void MainRegexxer::cbSearchReturnPressed(void) {
    btFindRegexClicked();
}

void MainRegexxer::cbReplaceReturnPressed(void) {
    btFindRegexClicked();
}

void MainRegexxer::cbSearchEditTextChanged(const QString& text) {
    ui_->btFind_->setDisabled(text.isEmpty());
}

void MainRegexxer::cbPatternsEditTextChanged(const QString& text) {
    ui_->btFindFiles_->setDisabled(text.isEmpty());
}

void MainRegexxer::cbPatternsCurrentIndexChanged(int index) {
    auto & box = ui_->cbPatterns_;

    // last element added
    if(index + 1 == box->count() && box->itemText(index) != last_pattern_) {
        // move to top
        auto text = box->itemText(index);
        box->removeItem(index);

        if(box->count() == 2) {
            box->insertSeparator(0);
        }

        box->insertItem(0, text, QVariant::fromValue(ListData(text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts), true)));
        box->setCurrentIndex(0);
    }
}

void MainRegexxer::filesAddItem(QTreeWidgetItem* item) {
    auto item_data = item->data(0, Qt::UserRole);
    auto path = qvariant_cast<QString>(item_data);

    // is file
    if(QFileInfo(path).isFile()) {
        QFile file(path);
        auto it = files_.insert(path, FileDataPtr());

        if(file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            it->reset(new FileData(file.readAll(), default_font_));
        } else {
            it->reset(new FileData());
            item->setText(1, "!");
            item->setData(1, Qt::UserRole, 0);
            item->setToolTip(1, file.errorString());
            item->setForeground(1, QBrush(Qt::red));
        }
    } else {
        for(int it = 0; it < item->childCount(); ++it) {
            filesAddItem(item->child(it));
        }
    }
}

void MainRegexxer::btFindFilesClicked(void) {
    bool any_changes = std::any_of(files_.begin(), files_.end(), [](auto & ptr) {
        return ptr->isChanges();
    });

    if(any_changes) {
        if(QMessageBox::No == QMessageBox::warning(this, "Sorry",
                tr("Some files haven't been saved yet.\nContinue anyway?"), QMessageBox::Yes | QMessageBox::No)) {
            return;
        }
    }

    auto folder_data = qvariant_cast<StringData>(ui_->cbFolders_->currentData());
    auto pattern_data = qvariant_cast<ListData>(ui_->cbPatterns_->currentData());
    bool recursive = ui_->cbRecursive_->checkState() == Qt::Checked;
    bool hidden = ui_->cbHidden_->checkState() == Qt::Checked;

    files_.clear();
    ui_->twFiles_->clear();

    if(auto items = makeFilesItems(folder_data.string(), recursive, hidden, pattern_data.list()); 0 < items.size()) {
        ui_->twFiles_->addTopLevelItems(items);

        for(auto & item : items) {
            filesAddItem(item);
        }
    }

    int files_count = files_.size();
    ui_->cbSearch_->setEnabled(0 < files_count);
    ui_->cbReplace_->setEnabled(0 < files_count);
    ui_->btFind_->setDisabled(ui_->cbSearch_->currentText().isEmpty());
    ui_->btPreviousFile_->setDisabled(true);
    ui_->btNextFile_->setDisabled(true);
}

void MainRegexxer::twFilesItemClicked(QTreeWidgetItem* item, int column) {

    ui_->leReplace_->setVisible(false);
    auto [data, path] = twItemFileData(item);

    // no file
    if(! data || ! data->isValid()) {
        file_matches_.first = 0;
        ui_->teContent_->setDocument(nullptr);
        updateStatus();
        return;
    }

    file_matches_.first = twItemsFiles().indexOf(item) + 1;

    // load content
    ui_->teContent_->setDocument(& data->document_);

    // set buttons
    ui_->btPrevMatch_->setEnabled(data->isCursorPrev());
    ui_->btNextMatch_->setEnabled(data->isCursorNext());
    ui_->actionSaveCurrent_->setEnabled(data->isChanges());

    ui_->btReplace_->setEnabled(0 < data->matches());
    ui_->btThisFile_->setEnabled(0 < data->matches());

    if(0 < data->matches()) {
        applyHighlight();
        applyCursor(data);
    }
    
    updateStatus(data);
}

QList<QTreeWidgetItem*> MainRegexxer::twItemsDirs(QTreeWidgetItem* item) {
    QList<QTreeWidgetItem*> res;

    if(item) {
        auto [data, path] = twItemFileData(item);

        if(! data) {
            for(int it = 0; it < item->childCount(); ++it) {
                res << twItemsDirs(item->child(it));
            }

            res << item;
        }
    } else {
        auto tw = ui_->twFiles_;

        for(int it = 0; it < tw->topLevelItemCount(); ++it) {
            res << twItemsDirs(tw->topLevelItem(it));
        }
    }

    return res;
}

QList<QTreeWidgetItem*> MainRegexxer::twItemsFiles(QTreeWidgetItem* item) {
    QList<QTreeWidgetItem*> res;

    if(item) {
        auto [data, path] = twItemFileData(item);

        // is file
        if(data && data->isValid()) {
            res << item;
        } else {
            for(int it = 0; it < item->childCount(); ++it) {
                res << twItemsFiles(item->child(it));
            }
        }
    } else {
        auto tw = ui_->twFiles_;

        for(int it = 0; it < tw->topLevelItemCount(); ++it) {
            res << twItemsFiles(tw->topLevelItem(it));
        }
    }

    return res;
}

QPair<FileData*, QString> MainRegexxer::twItemFileData(const QTreeWidgetItem* item) {
    if(! item) {
        return QPair<FileData*, QString>(nullptr, "");
    }

    if(auto item_data = item->data(0, Qt::UserRole); item_data.isValid()) {
        auto path = qvariant_cast<QString>(item_data);

        if(auto it = files_.find(path); it != files_.end()) {
            return QPair<FileData*, QString>(it->get(), path);
        }
    }

    return QPair<FileData*, QString>(nullptr, "");
}

QTreeWidgetItem* MainRegexxer::aboveItemMatches(const QTreeWidgetItem* item) {
    if(! item) {
        return nullptr;
    }

    auto items = twItemsFiles();
    auto it = std::find(items.begin(), items.end(), item);

    while(it != items.end() && it != items.begin()) {
        it = std::prev(it);

        // matches
        if(0 < qvariant_cast<int>((*it)->data(1, Qt::UserRole))) {
            return *it;
        }
    }

    return nullptr;
}

QTreeWidgetItem* MainRegexxer::belowItemMatches(const QTreeWidgetItem* item) {
    if(! item) {
        return nullptr;
    }

    auto items = twItemsFiles();
    auto it = std::find(items.begin(), items.end(), item);

    while(it != items.end() && std::next(it) != items.end()) {
        it = std::next(it);

        // matches
        if(0 < qvariant_cast<int>((*it)->data(1, Qt::UserRole))) {
            return *it;
        }
    }

    return nullptr;
}

void MainRegexxer::btPreviousFileClicked(void) {
    if(auto item = aboveItemMatches(ui_->twFiles_->currentItem())) {
        ui_->twFiles_->setCurrentItem(item);
        ui_->btPreviousFile_->setEnabled(aboveItemMatches(item));
        ui_->btNextFile_->setEnabled(true);
        twFilesItemClicked(item, 0);
        return;
    }

    ui_->btPreviousFile_->setDisabled(true);

    // not found, but..
    if(find_one_matches_) {
        find_one_matches_ = false;
        btNextFileClicked();
    }
}

void MainRegexxer::btNextFileClicked(void) {

    if(auto item = belowItemMatches(ui_->twFiles_->currentItem())) {
        ui_->twFiles_->setCurrentItem(item);
        ui_->btPreviousFile_->setEnabled(true);
        ui_->btNextFile_->setEnabled(belowItemMatches(item));
        twFilesItemClicked(item, 0);
        return;
    }

    ui_->btNextFile_->setDisabled(true);

    // not found, but..
    if(find_one_matches_) {
        find_one_matches_ = false;
        btPreviousFileClicked();
    }
}

FilesMatchesResult MainRegexxer::twItemsUpdateMatches(void) {
    FilesMatchesResult files_matches;

    // update files
    for(const auto & item : twItemsFiles()) {
        auto [data, path] = twItemFileData(item);

        if(data && data->isValid()) {
            data->updatePositions(regex_);
            item->setText(1, QString::number(data->matches()));
            item->setData(1, Qt::UserRole, data->matches());

            if(0 < data->matches()) {
                files_matches.first += 1;
                files_matches.second += data->matches();
            }
        }
    }

    // update dirs
    for(const auto & item : twItemsDirs()) {
        int folder_matches = 0;

        for(int it = 0; it < item->childCount(); it++) {
            auto child = item->child(it);
            folder_matches += qvariant_cast<int>(child->data(1, Qt::UserRole));
        }

        item->setText(1, QString::number(folder_matches));
        item->setData(1, Qt::UserRole, folder_matches);
    }

    return files_matches;
}

void MainRegexxer::twFilesItemChanged(QTreeWidgetItem* item, int column) {
}

void MainRegexxer::updateStatus(const FileData* data) {
    if(status_files_) {
        status_files_->setText(QString("File: %1/%2").
            arg(file_matches_.current()).arg(file_matches_.total()));
    }

    if(status_match_) {
        status_match_->setText(QString("Match: %1/%2").
            arg(data ? data->curpos_ + 1 : 0).arg(data ? data->matches() : 0));
    }
}

void MainRegexxer::btFindRegexClicked(void) {
    auto search = ui_->cbSearch_->currentText();

    // make regex
    QRegularExpression::PatternOptions opts = QRegularExpression::NoPatternOption;

    if(Qt::Checked == ui_->cbNoCase_->checkState()) {
        opts |= QRegularExpression::CaseInsensitiveOption;
    }

    regex_ = QRegularExpression(search, opts);

    FilesMatchesResult res = twItemsUpdateMatches();
    file_matches_.first = 0;
    file_matches_.second = res.files();

    ui_->btPreviousFile_->setEnabled(1 < res.files());
    ui_->btNextFile_->setEnabled(1 < res.files());
    ui_->btAllFiles_->setEnabled(0 < res.files());
    find_one_matches_ = false;

    if(auto item = ui_->twFiles_->currentItem()) {
        auto [data, path] = twItemFileData(item);

        // no file
        if(! data || ! data->isValid()) {
            return;
        }

        file_matches_.first = twItemsFiles().indexOf(item) + 1;

        ui_->btPrevMatch_->setDisabled(true);
        ui_->btNextMatch_->setEnabled(1 < data->matches());

        if(0 == data->matches()) {
            ui_->btPreviousFile_->setEnabled(true);
            ui_->btNextFile_->setEnabled(true);
            find_one_matches_ = true;
        }

        if(0 < data->matches()) {
            applyHighlight();
            applyCursor(data);
        }

        updateStatus(data);
    }
}

void MainRegexxer::applyCursor(FileData* data) {

    auto found = data->cursorPosition();

    if(! found) {
        ui_->leReplace_->setVisible(false);
        return;
    }

    auto cursor = ui_->teContent_->textCursor();

    cursor.setPosition(found->first, QTextCursor::MoveAnchor);
    cursor.setPosition(found->first + found->second, QTextCursor::KeepAnchor);

    ui_->teContent_->setTextCursor(cursor);
    ui_->leReplace_->setVisible(true);

    auto line = cursor.block().text();
    auto replace = ui_->cbReplace_->currentText();

    ui_->leReplace_->setText(line.replace(cursor.positionInBlock() - found->second, found->second, replace));
    ui_->btPrevMatch_->setEnabled(data->isCursorPrev());
    ui_->btNextMatch_->setEnabled(data->isCursorNext());
    ui_->btReplace_->setEnabled(0 < data->matches());
    ui_->btThisFile_->setEnabled(0 < data->matches());
}

void MainRegexxer::applyHighlight(void) {
    highlighter_.reset(new MultiWordHighlighter(ui_->teContent_, match_color_, regex_));
}

void MainRegexxer::btPrevMatchClicked(void) {
    ui_->btNextMatch_->setDisabled(false);

    auto [data, path] = twItemFileData(ui_->twFiles_->currentItem());

    if(data && data->isValid()) {
        if(0 < data->isCursorPrev()) {
            data->curpos_--;
            applyCursor(data);
        } else {
            ui_->btPrevMatch_->setDisabled(true);
        }

        updateStatus(data);
    }
}

void MainRegexxer::btNextMatchClicked(void) {
    ui_->btPrevMatch_->setDisabled(false);

    auto [data, path] = twItemFileData(ui_->twFiles_->currentItem());

    if(data && data->isValid()) {
        if(data->isCursorNext()) {
            data->curpos_++;
            applyCursor(data);
        } else {
            ui_->btNextMatch_->setDisabled(true);
        }

        updateStatus(data);
    }
}

void MainRegexxer::btReplaceClicked(void) {
    auto cursor = ui_->teContent_->textCursor();
    auto search = ui_->cbSearch_->currentText();
    auto replace = ui_->cbReplace_->currentText();

    cursor.beginEditBlock();
    cursor.removeSelectedText();
    cursor.insertText(replace);
    cursor.endEditBlock();

    auto item = ui_->twFiles_->currentItem();
    auto [data, path] = twItemFileData(item);

    if(data && data->isValid() && 0 < data->matches()) {
        bool last = data->isCursorLast();
        data->search_.removeAt(data->curpos_);

        if(last) {
            data->curpos_ = 0;
            ui_->btReplace_->setDisabled(true);
            ui_->leReplace_->setVisible(false);
        } else {
            data->search_ = searchPositions(data->document_.toPlainText(), regex_);
            twFilesItemClicked(item, 0);
        }

        if(0 == data->matches()) {
            file_matches_.first = 0;
            file_matches_.second -= 1;
        }

        ui_->actionSaveCurrent_->setEnabled(true);
        item->setText(1, QString::number(data->matches()));
        item->setData(1, Qt::UserRole, data->matches());

        twItemStyleChanged(item, true);
        updateStatus(data);
    }
}

void MainRegexxer::btThisFileClicked(void) {
    auto search = ui_->cbSearch_->currentText();
    auto replace = ui_->cbReplace_->currentText();
    auto item = ui_->twFiles_->currentItem();
    auto [data, path] = twItemFileData(item);

    if(data && data->isValid() && 0 < data->matches()) {
        auto content = data->document_.toPlainText();
        content.replace(regex_, replace);
        data->document_.setPlainText(content);
        data->clearPositions();

        ui_->actionSaveCurrent_->setEnabled(true);
        item->setText(1, QString::number(data->matches()));
        item->setData(1, Qt::UserRole, data->matches());

        file_matches_.first = 0;
        file_matches_.second -= 1;

        twItemStyleChanged(item, true);
        twFilesItemClicked(ui_->twFiles_->currentItem(), 0);
        updateStatus(data);
    }
}

void MainRegexxer::btAllFilesClicked(void) {
    auto search = ui_->cbSearch_->currentText();
    auto replace = ui_->cbReplace_->currentText();

    for(auto & item: twItemsFiles()) {
        auto [data, path] = twItemFileData(item);

        if(data && data->isValid() && 0 < data->matches()) {
            auto content = data->document_.toPlainText();
            content.replace(regex_, replace);
            data->document_.setPlainText(content);
            data->clearPositions();

            item->setText(1, QString::number(data->matches()));
            item->setData(1, Qt::UserRole, data->matches());
            twItemStyleChanged(item, true);
        }
    }

    ui_->btAllFiles_->setDisabled(true);
    ui_->actionSaveAll_->setEnabled(true);

    file_matches_.first = 0;
    file_matches_.second = 0;

    twFilesItemClicked(ui_->twFiles_->currentItem(), 0);
    updateStatus();
}

void MainRegexxer::twItemStyleChanged(QTreeWidgetItem* item, bool changed) {
    auto font = item->font(0);

    if(changed) {
        font.setItalic(true);
        item->setForeground(0, QBrush(Qt::red));
        item->setForeground(1, QBrush(Qt::red));
    } else {
        font.setItalic(false);
        auto palette = ui_->twFiles_->palette();
        item->setForeground(0, QBrush(palette.color(QPalette::WindowText)));
        item->setForeground(1, QBrush(palette.color(QPalette::WindowText)));
    }

    item->setFont(0, font);
    item->setFont(1, font);
}

void MainRegexxer::twItemFileSave(QTreeWidgetItem* item) {
    auto [data, path] = twItemFileData(item);

    if(data && data->isValid() && data->isChanges()) {
        QSaveFile file(path);

        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << file.errorString() << path;
            return;
        }

        QTextStream out(&file);

        out << data->document_.toPlainText();
        out.flush();

        if(file.error()) {
            qWarning() << file.errorString() << path;
            item->setText(1, "!");
            item->setToolTip(1, file.errorString());
            item->setForeground(1, QBrush(Qt::red));
        } else {
            file.commit();

            data->document_.setModified(false);
            item->setText(1, QString::number(data->matches()));
            item->setData(1, Qt::UserRole, data->matches());

            twItemStyleChanged(item, false);
        }
    }
}

void MainRegexxer::acSaveCurrentClicked(void) {
    twItemFileSave(ui_->twFiles_->currentItem());
    ui_->actionSaveCurrent_->setDisabled(true);
}

void MainRegexxer::acSaveAllClicked(void) {
    for(auto & item: twItemsFiles()) {
        twItemFileSave(item);
    }
    ui_->actionSaveCurrent_->setDisabled(true);
    ui_->actionSaveAll_->setDisabled(true);
}

void MainRegexxer::cbReplaceEditTextChanged(const QString& text) {

    auto [data, path] = twItemFileData(ui_->twFiles_->currentItem());

    if(data && data->isValid() && ui_->leReplace_->isVisible()) {
        applyCursor(data);
    }
}

void MainRegexxer::btHelpRegexClicked(void) {
    DialogHelp(this).exec();
}
