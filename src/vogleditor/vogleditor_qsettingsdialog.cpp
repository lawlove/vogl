#include "vogleditor_qsettingsdialog.h"
#include "ui_vogleditor_qsettingsdialog.h"

#include "vogleditor_settings.h"

#include <QScrollArea>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QStringList>

//static QScrollArea *scroller;
vogleditor_QSettingsDialog::vogleditor_QSettingsDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::vogleditor_QSettingsDialog)
{
    ui->setupUi(this);

    // Settings tab
    QString strSettings = g_settings.to_string();

    ui->textEdit->setText(strSettings);

    // Groups tab
#ifdef LLL
/*  QStringList debugMarkerList;
    QStringList nestOptionsList;

    debugMarkerList
        << "glPushDebugGroup/glPopDebugGroup"
        << "glPushGroupMarkerEXT/glPopGroupMarkerEXT";

    nestOptionsList
        << "glBegin/glEnd"
        << "glNewList/glEndList"
        << "glPushName/glPopName"
        << "glPushMatrix/glPopMatrix"
        << "glPushAttrib/glPopAttrib"
        << "glPushClientAttrib/glPopClientAttrib"; */
#endif // LLL

    // Get checkbox names and settings
#ifdef LLL
/*  QStringList   group_names = g_settings.group_names();
    QVector<bool> group_state = g_settings.group_settings();
    QVector<bool> group_allow = g_settings.group_enabled(); */
#endif // LLL
    QStringList   group_debug_marker_names = g_settings.group_debug_marker_names();
    QStringList   group_nest_options_names = g_settings.group_nest_options_names();

#ifdef LLL
    // State/Render
/*  m_pCheckboxStateRender = ui->checkboxStateRender;
    m_pCheckboxStateRender->setChecked(g_settings.groups_state_render()); */
#endif // LLL

    // State/Render
    m_pCheckboxStateRender = ui->checkboxStateRender;
    m_pCheckboxStateRender->setText(g_settings.group_state_render_name());
    m_pCheckboxStateRender->setChecked(g_settings.group_state_render_stat());
    m_pCheckboxStateRender->setEnabled(g_settings.group_state_render_used());

#ifdef LLL
    // Debug markers
/*  m_pCheckboxDebugMarker = new QCheckBox* [debugMarkerList.size()];
    for (int i=0; i < debugMarkerList.size(); i++)
    {
        m_pCheckboxDebugMarker[i] = new QCheckBox(debugMarkerList[i], ui->groupboxDebugMarker);
        ui->vLayout_groupboxDebugMarker->addWidget(m_pCheckboxDebugMarker[i]);
    }
    m_pCheckboxDebugMarker[0]->setChecked(g_settings.groups_push_pop_markers());
    m_pCheckboxDebugMarker[1]->setDisabled(true); */
#endif // LLL

    // Debug markers
    QVector<bool> debug_marker_stat = g_settings.group_debug_marker_stat();
    QVector<bool> debug_marker_used = g_settings.group_debug_marker_used();
    int debug_marker_cnt = group_debug_marker_names.size();
    for (int i=0; i < debug_marker_cnt; i++)
    {
        m_pCheckboxDebugMarker << new QCheckBox(group_debug_marker_names[i], ui->groupboxDebugMarker);
        m_pCheckboxDebugMarker[i]->setChecked(debug_marker_stat[i]);
        m_pCheckboxDebugMarker[i]->setEnabled(debug_marker_used[i]);
        ui->vLayout_groupboxDebugMarker->addWidget(m_pCheckboxDebugMarker[i]);
    }

#ifdef LLL
    // Nest options
/*  m_pGroupboxNestOptions = ui->groupboxNestOptions;
    m_pGroupboxNestOptions->setChecked(g_settings.groups_nested_calls());

    QVector<bool> checkboxValues(g_settings.group_settings());
    m_pCheckboxNestOptions = new QCheckBox* [nestOptionsList.size()];
    for (int i=0; i < nestOptionsList.size(); i++)
    {
        m_pCheckboxNestOptions[i] = new QCheckBox(nestOptionsList[i], ui->groupboxNestOptions);
        m_pCheckboxNestOptions[i]->setChecked(checkboxValues[i]);
        ui->verticalLayout_groupboxNestOptionsScrollarea->addWidget(m_pCheckboxNestOptions[i]);
    }
    m_pCheckboxNestOptions[0]->setChecked(true); */
#endif // LLL

    // Nest options
    m_pGroupboxNestOptions = ui->groupboxNestOptions;
    m_pGroupboxNestOptions->setChecked(true);

    QVector<bool> nest_options_stat = g_settings.group_nest_options_stat();
    QVector<bool> nest_options_used = g_settings.group_nest_options_used();
    int nest_options_cnt = group_nest_options_names.size();
    for (int i=0; i < nest_options_cnt; i++)
    {
        m_pCheckboxNestOptions << new QCheckBox(group_nest_options_names[i], ui->groupboxDebugMarker);
        m_pCheckboxNestOptions[i]->setChecked(nest_options_stat[i]);
        m_pCheckboxNestOptions[i]->setEnabled(nest_options_used[i]);
        ui->vLayout_groupboxNestOptionsScrollarea->addWidget(m_pCheckboxNestOptions[i]);
    }

#ifdef LLL
    // Connect checkbox callbacks
/*  connect(m_pCheckboxStateRender, SIGNAL(stateChanged(int)),
        SLOT(checkboxCB(int)));

    connect(m_pGroupboxNestOptions, SIGNAL(toggled(bool)),
        SLOT(groupboxCB(bool)));

    for (int i=0; i < debugMarkerList.size(); i++)
    {
        connect(m_pCheckboxDebugMarker[i], SIGNAL(stateChanged(int)),
            SLOT(checkboxCB(int)));
    } */
#endif // LLL

    // Connect checkbox callbacks

    // State Render
    connect(m_pCheckboxStateRender, SIGNAL(stateChanged(int)),
        SLOT(checkboxCB(int)));

    // Debug marker
    for (int i=0; i < debug_marker_cnt; i++)
    {
        connect(m_pCheckboxDebugMarker[i], SIGNAL(stateChanged(int)),
            SLOT(checkboxCB(int)));
    }

    // Nest options
    connect(m_pGroupboxNestOptions, SIGNAL(toggled(bool)),
        SLOT(groupboxCB(bool)));

    for (int i=0; i < nest_options_cnt; i++)
    {
        connect(m_pCheckboxNestOptions[i], SIGNAL(stateChanged(int)),
            SLOT(checkboxCB(int)));
    }

    m_bGroupInitialState = groupState();

} // constructor

vogleditor_QSettingsDialog::~vogleditor_QSettingsDialog()
{
    delete ui;

  //delete [] m_pCheckboxDebugMarker;
  //delete [] m_pCheckboxNestOptions;
    clearLayout(ui->verticalLayout_tabGroups);
} // destructor

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
} // clearLayout

void vogleditor_QSettingsDialog::groupboxCB(bool state)
{
    // update g_settings
#ifdef LLL
/*  g_settings.set_groups_state_render(m_pCheckboxStateRender->isChecked());
    g_settings.set_groups_push_pop_markers(m_pCheckboxDebugMarker[0]->isChecked());
    g_settings.set_groups_nested_calls(m_pGroupboxNestOptions->isChecked()); */
#endif // LLL

    // State/Render
    g_settings.set_group_state_render_stat(m_pCheckboxStateRender->isChecked());

#ifdef LLL
/* // get checkbox values 
    QVector<bool> bValues = checkboxValues();

    // set groups settings
    g_settings.set_group_settings(bValues); */
#endif // LLL
    // Debug marker
    QVector<bool> debug_marker_stat = checkboxValues(ui->groupboxDebugMarker);
    g_settings.set_group_debug_marker_stat(debug_marker_stat);

    // Nest options
    QVector<bool> nest_options_stat = checkboxValues(ui->groupboxNestOptions);
    g_settings.set_group_nest_options_stat(nest_options_stat);


    //update (first) tab settings page
    QString strSettings = g_settings.to_string();
    ui->textEdit->setText(strSettings);

} // groupboxCB

void vogleditor_QSettingsDialog::checkboxCB(int state)
{
    groupboxCB(bool(state));

} // checkboxCB

QVector<bool> vogleditor_QSettingsDialog::checkboxValues(QGroupBox *groupBox)
{
    QList<QCheckBox*> groupCheckBoxes = groupBox->findChildren<QCheckBox*>();

  //QStringList checkboxNames;
    QVector<bool> bQVector;
    QList<QCheckBox*>::const_iterator iter = groupCheckBoxes.begin();

    while (iter != groupCheckBoxes.end()) {
     // checkboxNames << (*iter)->text();
        bQVector << (*iter)->isChecked();
        iter++;
    }
    return bQVector;

} // checkboxValues()

QVector<bool> vogleditor_QSettingsDialog::groupState()
{
    QVector<bool> bCurrentState;

    bCurrentState << m_pCheckboxStateRender->isChecked();
    bCurrentState << m_pGroupboxNestOptions->isChecked();

    for (int i=0, cnt=m_pCheckboxDebugMarker.size(); i < cnt; i++)
    {
        bCurrentState << m_pCheckboxDebugMarker[i]->isChecked();
    }
    for (int i=0, cnt=m_pCheckboxNestOptions.size(); i < cnt; i++)
    {
        bCurrentState << m_pCheckboxNestOptions[i]->isChecked();
    }
    return bCurrentState;
}

bool vogleditor_QSettingsDialog::groupOptionsChanged()
{
    return m_bGroupInitialState != groupState();
}

void vogleditor_QSettingsDialog::save(const char* settingsFile)
{
    g_settings.from_string(ui->textEdit->toPlainText().toStdString().c_str());
    g_settings.save(settingsFile);

} // save()

