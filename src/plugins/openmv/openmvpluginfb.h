#ifndef OPENMVPLUGINFB_H
#define OPENMVPLUGINFB_H

#include <QtCore>
#include <QtGui>
#include <QtWidgets>

class OpenMVPluginFB : public QGraphicsView
{
    Q_OBJECT

public:

    explicit OpenMVPluginFB(QWidget *parent = Q_NULLPTR);

public slots:

    void enableFitInView(bool enable);
    void frameBufferData(const QPixmap &data);

signals:

    void pixmapUpdate(const QPixmap &data);
    void saveImage(const QPixmap &data);
    void saveTemplate(const QRect &rect);
    void saveDescriptor(const QRect &rect);

protected:

    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void contextMenuEvent(QContextMenuEvent *event);
    virtual void resizeEvent(QResizeEvent *event);

private:

    QPixmap getPixmap(bool pointValid = false, const QPoint &point = QPoint(), bool *cropped = Q_NULLPTR, QRect *croppedRect = Q_NULLPTR);
    void myFitInView();

    bool m_enableFitInView;
    QGraphicsPixmapItem *m_pixmap;

    bool m_unlocked;
    QPoint m_origin;
    QRubberBand *m_band;
};

#endif // OPENMVPLUGINFB_H
