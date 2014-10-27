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
    QStringList markList;
    markList << "glPushDebugGroup/glPopDebugGroup"
             << "glPushGroupMarkerEXT/glPopGroupMarkerEXT";
    m_debugMarkerListSize = markList.count();

    QStringList nestList;
    nestList << "glBegin/glEnd"
             << "glNewList/glEndList"
             << "glPushName/glPopName"
             << "glPushMatrix/glPopMatrix"
             << "glPushAttrib/glPopAttrib"
             << "glPushClientAttrib/glPopClientAttrib";
    m_nestedCallsListSize = markList.count();

    m_vlayoutTotal = new QVBoxLayout(ui->tab_2);

    // State/Render checkbox
    m_checkboxStateRender = new QCheckBox("State/Render groups", ui->tab_2);
    m_checkboxStateRender->setChecked(g_settings.groups_state_render());

    // Debug marker groupbox
    m_groupboxDebugMarkers = new QGroupBox("Debug marker groups", ui->tab_2);
    m_vlayoutGroupBoxMarkers = new QVBoxLayout(m_groupboxDebugMarkers);

    m_checkboxMarkList = new QCheckBox* [markList.size()];
    for (int i=0; i < markList.size(); i++)
    {
        m_vlayoutGroupBoxMarkers->addWidget(m_checkboxMarkList[i] = new QCheckBox(markList[i]));
    }
    m_checkboxMarkList[0]->setChecked(g_settings.groups_push_pop_markers());
    m_checkboxMarkList[1]->setDisabled(true);

    // Nest Options groupbox
    m_groupboxNestOptions = new QGroupBox("Nest options", ui->tab_2);
    m_vlayoutGroupBoxNestOptions = new QVBoxLayout(m_groupboxNestOptions);
    m_scrollareaGroupBoxNestOptions = new QScrollArea(m_groupboxNestOptions);
    m_widgetNestOptions = new QWidget(m_groupboxNestOptions);
    m_vlayoutNestOptions = new QVBoxLayout(m_widgetNestOptions);

    m_groupboxNestOptions->setCheckable(true);

    m_checkboxNestList = new QCheckBox* [nestList.size()];
    for (int i=0; i < nestList.size(); i++)
    {
        m_vlayoutNestOptions->addWidget(m_checkboxNestList[i] = new QCheckBox(nestList[i]));
    }
    m_checkboxNestList[0]->setChecked(true);

    // Scroll the nest options
    m_scrollareaGroupBoxNestOptions->setWidgetResizable(true);
    m_scrollareaGroupBoxNestOptions->setWidget(m_widgetNestOptions);
    m_vlayoutGroupBoxNestOptions->addWidget(m_scrollareaGroupBoxNestOptions);

    // Put it all together
    m_vlayoutTotal->addWidget(m_checkboxStateRender);
    m_vlayoutTotal->addWidget(m_groupboxDebugMarkers);
    m_vlayoutTotal->addWidget(m_groupboxNestOptions);


    connect(m_checkboxStateRender, SIGNAL(stateChanged(int)),
        SLOT(checkboxCB(int)));

    for (int i=0; i < markList.size(); i++)
    {
        connect(m_checkboxMarkList[i], SIGNAL(stateChanged(int)),
            SLOT(checkboxCB(int)));
    }
}

    int groupsDebugMarkersListSize();
int vogleditor_QSettingsDialog::groupsDebugMarkersListSize()
{
    return m_debugMarkersListSize;
}
int vogleditor_QSettingsDialog::groupsNestedCallsListSize();
{
    return m_nestedCallsListSize;
}
vogleditor_QSettingsDialog::~vogleditor_QSettingsDialog()
{
    delete ui;

    delete [] m_checkboxMarkList;
    delete [] m_checkboxNestList;
    clearLayout(m_vlayoutTotal);
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
        g_settings.set_groups_state_render(m_checkboxStateRender->isChecked());
        g_settings.set_groups_push_pop_markers(m_checkboxMarkList[0]->isChecked());

        //update (first) tab settings page
        QString strSettings = g_settings.to_string();
        ui->textEdit->setText(strSettings);
}
void vogleditor_QSettingsDialog::save(const char* settingsFile)
{
    g_settings.from_string(ui->textEdit->toPlainText().toStdString().c_str());
    g_settings.save(settingsFile);
}

