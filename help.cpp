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

#include <QKeyEvent>
#include <QSettings>

#include "help.h"
#include "ui_help.h"

DialogHelp::DialogHelp(QWidget* parent) :
    QDialog(parent), ui_(new Ui::help) {
    ui_->setupUi(this);

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "qregexxer", "settings");

    if(settings.contains("help_geometry")) {
        setGeometry(qvariant_cast<QRect>(settings.value("help_geometry")));
    }
}

void DialogHelp::closeEvent(QCloseEvent* ev) {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "qregexxer", "settings");
    settings.setValue("help_geometry", geometry());
}

void DialogHelp::keyPressEvent(QKeyEvent* ev) {
    if(ev->key() == Qt::Key_Escape ||
       ev->key() == Qt::Key_Return || ev->key() == Qt::Key_Space) {
        close();
    }
}
