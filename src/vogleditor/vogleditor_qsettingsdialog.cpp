#include "vogleditor_qsettingsdialog.h"
#include "ui_vogleditor_qsettingsdialog.h"

#include "vogleditor_settings.h"

#include <QScrollArea>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QStringList>

//static QScrollArea *scroller;
vogleditor_QSettingsDialog::vogleditor_QSettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::vogleditor_QSettingsDialog)
{
    ui->setupUi(this);

    // Settings tab
    QString strSettings = g_settings.to_string();

    ui->textEdit->setText(strSettings);

    // Groups tab
    QStringList debugMarkerList;
    QStringList nestOptionsList;

    debugMarkerList
        << "glPushDebugGroup/glPopDebugGroup"
        << "glPushGroupMarkerEXT/glPopGroupMarkerEXT";
    m_debugMarkerListSize = debugMarkerList.count();

    nestOptionsList
        << "glBegin/glEnd"
        << "glNewList/glEndList"
        << "glPushName/glPopName"
        << "glPushMatrix/glPopMatrix"
        << "glPushAttrib/glPopAttrib"
        << "glPushClientAttrib/glPopClientAttrib";
    m_nestOptionsListSize = nestOptionsList.count();

    m_pCheckboxStateRender = ui->checkboxStateRender;
    m_pCheckboxStateRender->setChecked(g_settings.groups_state_render());

    m_pCheckboxDebugMarker = new QCheckBox* [debugMarkerList.size()];
    for (int i=0; i < debugMarkerList.size(); i++)
    {
        m_pCheckboxDebugMarker[i] = new QCheckBox(debugMarkerList[i], ui->groupboxDebugMarker);
        ui->vLayout_groupboxDebugMarker->addWidget(m_pCheckboxDebugMarker[i]);
    }
    m_pCheckboxDebugMarker[0]->setChecked(g_settings.groups_push_pop_markers());
    m_pCheckboxDebugMarker[1]->setDisabled(true);

    m_pCheckboxNestOptions = new QCheckBox* [nestOptionsList.size()];
    for (int i=0; i < nestOptionsList.size(); i++)
    {
        m_pCheckboxNestOptions[i] = new QCheckBox(nestOptionsList[i], ui->groupboxNestOptions);
        ui->verticalLayout_groupboxNestOptionsScrollarea->addWidget(m_pCheckboxNestOptions[i]);
    }
    m_pCheckboxNestOptions[0]->setChecked(true);

    connect(m_pCheckboxStateRender, SIGNAL(stateChanged(int)),
        SLOT(checkboxCB(int)));

    for (int i=0; i < debugMarkerList.size(); i++)
    {
        connect(m_pCheckboxDebugMarker[i], SIGNAL(stateChanged(int)),
            SLOT(checkboxCB(int)));
    }
}

int vogleditor_QSettingsDialog::groupsDebugMarkerListSize()
{
    return m_debugMarkerListSize;
}
int vogleditor_QSettingsDialog::groupsNestOptionsListSize()
{
    return m_nestOptionsListSize;
}
vogleditor_QSettingsDialog::~vogleditor_QSettingsDialog()
{
    delete ui;

    delete [] m_pCheckboxDebugMarker;
    delete [] m_pCheckboxNestOptions;
    clearLayout(ui->verticalLayout_tabGroups);
}

void vogleditor_QSettingsDialog::clearLayout(QLayout *layout)
{
// taken from
// http://stackoverflow.com/questions/4272196/qt-remove-all-widgets-from-layout
// ... didn't seem to make any difference using valgrind Memcheck...

    while (QLayoutItem *item = layout->takeAt(0))
    {
        delete item->widget();
        if (QLayout *childLayout = item->layout())
            clearLayout(childLayout);
        delete item;
    }
}

void vogleditor_QSettingsDialog::checkboxCB(int state)
{
        // update g_settings
        g_settings.set_groups_state_render(m_pCheckboxStateRender->isChecked());
        g_settings.set_groups_push_pop_markers(m_pCheckboxDebugMarker[0]->isChecked());

        //update (first) tab settings page
        QString strSettings = g_settings.to_string();
        ui->textEdit->setText(strSettings);
}
void vogleditor_QSettingsDialog::save(const char* settingsFile)
{
    g_settings.from_string(ui->textEdit->toPlainText().toStdString().c_str());
    g_settings.save(settingsFile);
}

