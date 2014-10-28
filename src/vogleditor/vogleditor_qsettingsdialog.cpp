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

    //m_vlayoutTotal = new QVBoxLayout(ui->tab_2);

    // State/Render checkbox
    //m_checkboxStateRender = new QCheckBox("State/Render groups", ui->tab_2);
    //m_checkboxStateRender->setChecked(g_settings.groups_state_render());

    // Debug marker groupbox
    //m_groupboxDebugMarkers = new QGroupBox("Debug marker groups", ui->tab_2);
    //m_vlayoutGroupBoxMarkers = new QVBoxLayout(m_groupboxDebugMarkers);

    m_checkboxStateRender = ui->checkBox;
    m_checkboxStateRender->setChecked(g_settings.groups_state_render());

    m_checkboxDebugMarker = new QCheckBox* [debugMarkerList.size()];
    for (int i=0; i < debugMarkerList.size(); i++)
    {
        //m_vlayoutGroupBoxMarkers->addWidget(m_checkboxDebugMarker[i] = new QCheckBox(debugMarkerList[i]));
        m_checkboxDebugMarker[i] = new QCheckBox(debugMarkerList[i], ui->groupBox);
        ui->verticalLayout_4->addWidget(m_checkboxDebugMarker[i]);
    }
    m_checkboxDebugMarker[0]->setChecked(g_settings.groups_push_pop_markers());
    m_checkboxDebugMarker[1]->setDisabled(true);

    // Nest Options groupbox
    //m_groupboxNestOptions = new QGroupBox("Nest options", ui->tab_2);
    //m_vlayoutGroupBoxNestOptions = new QVBoxLayout(m_groupboxNestOptions);
    //m_scrollareaGroupBoxNestOptions = new QScrollArea(m_groupboxNestOptions);
    //m_widgetNestOptions = new QWidget(m_groupboxNestOptions);
    //m_vlayoutNestOptions = new QVBoxLayout(m_widgetNestOptions);

    //m_groupboxNestOptions->setCheckable(true);

    m_checkboxNestOptions = new QCheckBox* [nestOptionsList.size()];
    for (int i=0; i < nestOptionsList.size(); i++)
    {
        //m_vlayoutNestOptions->addWidget(m_checkboxnestOptions[i] = new QCheckBox(nestOptionsList[i]));
        m_checkboxNestOptions[i] = new QCheckBox(nestOptionsList[i], ui->groupBox_2);
        ui->verticalLayout_6->addWidget(m_checkboxNestOptions[i]);
    }
    m_checkboxNestOptions[0]->setChecked(true);

    // Scroll the nest options
    //m_scrollareaGroupBoxNestOptions->setWidgetResizable(true);
    //m_scrollareaGroupBoxNestOptions->setWidget(m_widgetNestOptions);
    //m_vlayoutGroupBoxNestOptions->addWidget(m_scrollareaGroupBoxNestOptions);

    // Put it all together
    //m_vlayoutTotal->addWidget(m_checkboxStateRender);
    //m_vlayoutTotal->addWidget(m_groupboxDebugMarkers);
    //m_vlayoutTotal->addWidget(m_groupboxNestOptions);



    connect(m_checkboxStateRender, SIGNAL(stateChanged(int)),
        SLOT(checkboxCB(int)));

    for (int i=0; i < debugMarkerList.size(); i++)
    {
        connect(m_checkboxDebugMarker[i], SIGNAL(stateChanged(int)),
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

    delete [] m_checkboxDebugMarker;
    delete [] m_checkboxNestOptions;
    //clearLayout(ui->verticalLayout_3);
}

#ifdef LLL
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
#endif //LL

void vogleditor_QSettingsDialog::checkboxCB(int state)
{
        // update g_settings
        g_settings.set_groups_state_render(m_checkboxStateRender->isChecked());
        g_settings.set_groups_push_pop_markers(m_checkboxDebugMarker[0]->isChecked());

        //update (first) tab settings page
        QString strSettings = g_settings.to_string();
        ui->textEdit->setText(strSettings);
}
void vogleditor_QSettingsDialog::save(const char* settingsFile)
{
    g_settings.from_string(ui->textEdit->toPlainText().toStdString().c_str());
    g_settings.save(settingsFile);
}

