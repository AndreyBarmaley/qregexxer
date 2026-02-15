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

#include "regexxer.h"

#include <QDebug>
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    QCoreApplication::setApplicationName("qregexxer");
    QCoreApplication::setApplicationVersion(QString::number(20260207));

    QCommandLineParser parser;

    parser.setApplicationDescription("\nqregexxer is a nifty search/replace tool for the desktop user.\n"
                                     "It features recursive search through directory trees and QRegExp-style regular expressions.");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addOptions({
        { { "R", "no-recursion" }, QCoreApplication::translate("main", "Do not recurse into subdirectories")},
        { { "d", "hidden" }, QCoreApplication::translate("main", "Also find hidden files")},
        { { "G", "no-global" }, QCoreApplication::translate("main", "Find only the first match in a line")},
        { { "i", "ignore-case" }, QCoreApplication::translate("main", "Do case insensitive matching")},
        //{ { "n", "line-number" }, QCoreApplication::translate("main", "Print match location to standard output")},
        //{ { "A", "no-autorun" }, QCoreApplication::translate("main", "Do not automatically start search")},
        { { "p", "pattern" }, QCoreApplication::translate("main", "Find files matching <PATTERN>"), "PATTERN"},
        { { "e", "regex" }, QCoreApplication::translate("main", "Find text matching <REGEX>"), "REGEX"},
        { { "s", "substitution" }, QCoreApplication::translate("main", "Replace matches with <STRING>"), "STRING"},
    });

    parser.addPositionalArgument("folder", QCoreApplication::translate("main", "folder to open, optionally."), "[folder...]");

    if(! parser.parse(app.arguments())) {
        qWarning() << parser.errorText();
        return EXIT_FAILURE;
    }

    parser.process(app);

    MainRegexxer win(parser);
    win.show();

    return app.exec();
}
