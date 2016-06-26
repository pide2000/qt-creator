#ifndef OPENMVFRAMEBUFFER_H
#define OPENMVFRAMEBUFFER_H

#include <QtCore>
#include <QtGui>
#include <QtWidgets>

class OpenMVFrameBuffer : public QGraphicsView
{
    Q_OBJECT

public:

    explicit OpenMVFrameBuffer(QWidget *parent = 0);

protected:

};

#endif // OPENMVFRAMEBUFFER_H
