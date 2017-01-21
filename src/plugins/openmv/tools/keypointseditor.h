#ifndef KEYPOINTSEDITOR_H
#define KEYPOINTSEDITOR_H

#include <QDialog>

namespace Ui {
class KeypointsEditor;
}

class KeypointsEditor : public QDialog
{
    Q_OBJECT

public:
    explicit KeypointsEditor(QWidget *parent = 0);
    ~KeypointsEditor();

private:
    Ui::KeypointsEditor *ui;
};

#endif // KEYPOINTSEDITOR_H
