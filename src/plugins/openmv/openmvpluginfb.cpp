#include "openmvpluginfb.h"

OpenMVPluginFB::OpenMVPluginFB(QWidget *parent) : QGraphicsView(parent)
{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameStyle(QFrame::NoFrame);
    setMinimumWidth(160);
    setMinimumHeight(120);
    setBackgroundBrush(QColor(30, 30, 39));
    setScene(new QGraphicsScene(this));

    QGraphicsTextItem *item = new QGraphicsTextItem;
    item->setHtml(tr("<html><body style=\"color:#909090; font-size:14px\">"
    "<div align='center'>"
    "<div style=\"font-size:20px\">No Image</div>"
    "</div>"
    "</body></html>"));
    scene()->addItem(item);

    m_enableFitInView = false;
    m_pixmap = Q_NULLPTR;
    m_enableSaveTemplate = false;
    m_enableSaveDescriptor = false;
    m_unlocked = false;
    m_origin = QPoint();

    m_band = new QRubberBand(QRubberBand::Rectangle, this);
    m_band->setGeometry(QRect());
    m_band->hide();
}

void OpenMVPluginFB::enableFitInView(bool enable)
{
    m_enableFitInView = enable;

    if(m_pixmap)
    {
        myFitInView();
    }

    if(m_band->isVisible())
    {
        m_unlocked = false;
        m_band->setGeometry(QRect());
        m_band->hide();

        // Broadcast the new pixmap
        emit pixmapUpdate(getPixmap());
    }
}

void OpenMVPluginFB::frameBufferData(const QPixmap &data)
{
    delete scene();
    setScene(new QGraphicsScene(this));

    m_pixmap = scene()->addPixmap(data);

    myFitInView();

    // Broadcast the new pixmap
    emit pixmapUpdate(getPixmap());
}

void OpenMVPluginFB::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        m_unlocked = m_pixmap && (m_pixmap == itemAt(event->pos()));
        m_origin = event->pos();
        m_band->setGeometry(QRect());
        m_band->hide();

        // Broadcast the new pixmap
        emit pixmapUpdate(getPixmap());
    }

    QGraphicsView::mousePressEvent(event);
}

void OpenMVPluginFB::mouseMoveEvent(QMouseEvent *event)
{
    if(m_unlocked)
    {
        m_band->setGeometry(QRect(m_origin, event->pos()).normalized().intersected(mapFromScene(sceneRect()).boundingRect()));
        m_band->show();

        // Broadcast the new pixmap
        emit pixmapUpdate(getPixmap());
    }

    QGraphicsView::mouseMoveEvent(event);
}

void OpenMVPluginFB::mouseReleaseEvent(QMouseEvent *event)
{
    m_unlocked = false;

    QGraphicsView::mouseReleaseEvent(event);
}

void OpenMVPluginFB::contextMenuEvent(QContextMenuEvent *event)
{
    if(m_pixmap && (m_pixmap == itemAt(event->pos())))
    {
        bool cropped;
        QRect croppedRect;
        QPixmap pixmap = getPixmap(true, event->pos(), &cropped, &croppedRect);

        QMenu menu(this);

        QAction *sImage = menu.addAction(cropped ? tr("Save Image selection to PC") : tr("Save Image to PC"));
        menu.addSeparator();
        QAction *sTemplate = menu.addAction(cropped ? tr("Save Template selection to Cam") : tr("Save Template to Cam"));
        sTemplate->setVisible(m_enableSaveTemplate);
        QAction *sDescriptor = menu.addAction(cropped ? tr("Save Descriptor selection to Cam") : tr("Save Descriptor to Cam"));
        sDescriptor->setVisible(m_enableSaveDescriptor);

        QAction *selected = menu.exec(event->globalPos());

        if(selected == sImage)
        {
            emit saveImage(pixmap);
        }
        else if(selected == sTemplate)
        {
            emit saveTemplate(croppedRect);
        }
        else if(selected == sDescriptor)
        {
            emit saveDescriptor(croppedRect);
        }
    }
}

void OpenMVPluginFB::resizeEvent(QResizeEvent *event)
{
    if(m_pixmap)
    {
        myFitInView();
    }

    if(m_band->isVisible())
    {
        m_unlocked = false;
        m_band->setGeometry(QRect());
        m_band->hide();

        // Broadcast the new pixmap
        emit pixmapUpdate(getPixmap());
    }

    QGraphicsView::resizeEvent(event);
}

QPixmap OpenMVPluginFB::getPixmap(bool pointValid, const QPoint &point, bool *cropped, QRect *croppedRect)
{
    if(!m_pixmap)
    {
        return QPixmap();
    }

    bool crop = m_band->geometry().isValid() && ((!pointValid) || m_band->geometry().contains(point));

    if(cropped)
    {
        *cropped = crop;
    }

    QRect rect = m_pixmap->mapFromScene(mapToScene(m_band->geometry())).boundingRect().toRect();

    if(croppedRect)
    {
        *croppedRect = crop ? rect : m_pixmap->pixmap().rect();
    }

    if(crop)
    {
        return m_pixmap->pixmap().copy(rect);
    }

    return m_pixmap->pixmap();
}

void OpenMVPluginFB::myFitInView()
{
    qreal scale = qMin(width() / sceneRect().width(), height() / sceneRect().height());
    QTransform matrix(1, 0, 0, 0, 1, 0, 0, 0, 1);

    if(m_enableFitInView)
    {
        matrix.scale(scale, scale);
    }

    setTransform(matrix);
}
