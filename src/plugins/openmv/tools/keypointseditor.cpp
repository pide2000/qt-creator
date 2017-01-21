#include "keypointseditor.h"
#include "ui_keypointseditor.h"

KeypointsEditor::KeypointsEditor(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::KeypointsEditor)
{
    ui->setupUi(this);
}

KeypointsEditor::~KeypointsEditor()
{
    delete ui;
}
