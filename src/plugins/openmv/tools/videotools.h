#ifndef VIDEOTOOLS_H
#define VIDEOTOOLS_H

#include <QtCore>
#include <QtGui>
#include <QtWidgets>

#include <coreplugin/icore.h>
#include <extensionsystem/pluginmanager.h>
#include <utils/synchronousprocess.h>

#include "../openmvpluginio.h"

void convertVideoFileAction(const QString &drivePath);
void playVideoFileAction(const QString &drivePath);
bool convertVideoFile(const QString &dst, const QString &src);
bool playVideoFile(const QString &path);

#endif // VIDEOTOOLS_H
