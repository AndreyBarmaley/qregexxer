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

#ifndef REGEXXER_H
#define REGEXXER_H

#include <QMap>
#include <QList>
#include <QFont>
#include <QString>
#include <QSettings>
#include <QMainWindow>
#include <QTextDocument>
#include <QSharedPointer>
#include <QScopedPointer>
#include <QRegularExpression>
#include <QSyntaxHighlighter>

#include "ui_regexxer.h"

class QDir;
class QLabel;
class QColor;
class QAction;
class QKeyEvent;
class QTextEdit;
class QCloseEvent;
class QTreeWidgetItem;
class QCommandLineParser;

namespace Ui {
    class regexxer;
}

struct FilesMatchesResult : QPair<int, int> {
    int files(void) const { return first; }
    int matches(void) const { return second; }

    FilesMatchesResult & operator +=(const FilesMatchesResult &);
};

struct CurrentTotal : QPair<int, int> {
    int current(void) const { return first; }
    int total(void) const { return second; }
};

using OffsetLength = QPair<int, int>;

struct FileData {
    QTextDocument document_;
    QList<OffsetLength> search_;

    int curpos_ = 0;
    bool is_error_ = true;

    bool isValid(void) const;
    bool isChanges(void) const;
    bool isCursorPrev(void) const;
    bool isCursorNext(void) const;
    bool isCursorLast(void) const;
    int matches(void) const;

    const OffsetLength* cursorPosition(void) const;
    void updatePositions(const QRegularExpression &);
    void clearPositions(void);

    FileData(const QString & content, const QFont & font) : document_(content), is_error_(false) {
        document_.setDefaultFont(font);
        document_.setModified(false);
    }

    FileData() = default;
    ~FileData() = default;
};

using FileDataPtr = QSharedPointer<FileData>;

class MainRegexxer : public QMainWindow {
    Q_OBJECT

    QMap<QString, FileDataPtr> files_;

    QSettings settings_;
    const QString last_pattern_;
    QScopedPointer<Ui::regexxer> ui_;
    QScopedPointer<QSyntaxHighlighter> highlighter_;
    QRegularExpression regex_;

    QFont default_font_;
    QColor match_color_;
    QColor current_color_;

    QLabel* status_files_ = nullptr;
    QLabel* status_match_ = nullptr;

    CurrentTotal file_matches_;

    bool find_one_matches_ = false;

  private:
    void filesAddItem(QTreeWidgetItem*);
    void applyHighlight(void);
    void applyCursor(FileData*);
    void updateStatus(const FileData* = nullptr);

    QList<QTreeWidgetItem*> twItemsDirs(QTreeWidgetItem* = nullptr);
    QList<QTreeWidgetItem*> twItemsFiles(QTreeWidgetItem* = nullptr);

    void twItemFileSave(QTreeWidgetItem*);
    void twItemStyleChanged(QTreeWidgetItem*, bool changed);
    QPair<FileData*, QString> twItemFileData(const QTreeWidgetItem*);
    QTreeWidgetItem* aboveItemMatches(const QTreeWidgetItem*);
    QTreeWidgetItem* belowItemMatches(const QTreeWidgetItem*);

    QList<QTreeWidgetItem*> makeFilesItems(const QDir & dir, bool recursive, bool hidden, const QStringList & filters) const;
    FilesMatchesResult twItemsUpdateMatches(void);

    void loadCbFolders(QComboBox &, QStringList dirs) const;
    void loadCbPatterns(QComboBox &, QStringList vals) const;

  protected slots:
    void cbFoldersActivated(int);
    void cbPatternsCurrentIndexChanged(int);
    void cbPatternsEditTextChanged(const QString &);
    void cbSearchReturnPressed(void);
    void cbReplaceReturnPressed(void);
    void cbSearchEditTextChanged(const QString &);
    void cbReplaceEditTextChanged(const QString &);
    void btFindFilesClicked(void);
    void btFindRegexClicked(void);
    void btPreviousFileClicked(void);
    void btNextFileClicked(void);
    void btPrevMatchClicked(void);
    void btNextMatchClicked(void);
    void twFilesItemClicked(QTreeWidgetItem*, int column);
    void twFilesItemChanged(QTreeWidgetItem*, int column);
    void dialogOpenDirectory(void);
    void btReplaceClicked(void);
    void btThisFileClicked(void);
    void btAllFilesClicked(void);
    void acSaveCurrentClicked(void);
    void acSaveAllClicked(void);
    void btHelpRegexClicked(void);

  protected:
    void closeEvent(QCloseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

  public:
    MainRegexxer(const QCommandLineParser &);
    ~MainRegexxer() = default;
};
#endif // REGEXXER_H
