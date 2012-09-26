/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2012 AudioCodes Ltd.
**
** Author: Orgad Shaneh <orgad.shaneh@audiocodes.com>
**
** Contact: http://www.qt-project.org/
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**************************************************************************/

#ifndef CHECKOUTDIALOG_H
#define CHECKOUTDIALOG_H

#include <QDialog>

namespace ClearCase {
namespace Internal {

namespace Ui {
    class CheckOutDialog;
}

class ActivitySelector;

class CheckOutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CheckOutDialog(const QString &fileName, bool isUcm, QWidget *parent = 0);
    ~CheckOutDialog();
    QString activity() const;
    QString comment() const;
    bool isReserved() const;
    bool isUnreserved() const;
    bool isPreserveTime() const;
    bool isUseHijacked() const;
    void hideHijack();

private slots:
    void toggleUnreserved(bool checked);

private:
    Ui::CheckOutDialog *ui;
    ActivitySelector *m_actSelector;
};

} // namespace Internal
} // namespace ClearCase

#endif // CHECKOUTDIALOG_H
