#ifndef OPENMVPLUGINFB_H
#define OPENMVPLUGINFB_H

#include <QtCore>
#include <QtGui>
#include <QtWidgets>

class OpenMVPluginFB : public QGraphicsView
{
    Q_OBJECT

public:

    explicit OpenMVPluginFB(QWidget *parent = nullptr);

public slots:

    void frameBufferData(const QPixmap &data);

private:

    QGraphicsScene *m_scene;

};

#endif // OPENMVPLUGINFB_H
