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

#ifndef HELP_H
#define HELP_H

#include <QDialog>
#include "ui_help.h"

namespace Ui {
    class help;
}

class DialogHelp : public QDialog {
    Q_OBJECT

    QScopedPointer<Ui::help> ui_;

  public:
    explicit DialogHelp(QWidget* parent = 0);

  protected:
    void closeEvent(QCloseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
};

#endif // HELP_H
