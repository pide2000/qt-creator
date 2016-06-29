#include "openmvpluginfb.h"

OpenMVPluginFB::OpenMVPluginFB(QWidget *parent) : QGraphicsView(parent)
{
    m_scene = new QGraphicsScene(this);
    m_scene->setBackgroundBrush(QColor(30, 30, 39));
    setScene(m_scene);

    QGraphicsTextItem *item = new QGraphicsTextItem;
    item->setHtml(tr("<html><body style=\"color:#909090; font-size:14px\">"
    "<div align='center'>"
    "<div style=\"font-size:20px\">No Image</div>"
    "</div>"
    "</body></html>"));
    m_scene->addItem(item);
    centerOn(item);
}

void OpenMVPluginFB::frameBufferData(const QPixmap &data)
{
    m_scene->clear();
    QGraphicsPixmapItem *item = m_scene->addPixmap(data);
    fitInView(item, Qt::KeepAspectRatio);
    centerOn(item);
}
