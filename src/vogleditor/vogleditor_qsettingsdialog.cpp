#include "vogleditor_qsettingsdialog.h"
#include "ui_vogleditor_qsettingsdialog.h"

#include "vogleditor_settings.h"

vogleditor_QSettingsDialog::vogleditor_QSettingsDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::vogleditor_QSettingsDialog)
{
    ui->setupUi(this);

    // Settings tab
    QString strSettings = g_settings.to_string();

    ui->textEdit->setText(strSettings);

    // Groups tab

    // State/Render
    m_pCheckboxStateRender = ui->checkboxStateRender;
    m_pCheckboxStateRender->setText(g_settings.group_state_render_name());
    m_pCheckboxStateRender->setChecked(g_settings.group_state_render_stat());
    m_pCheckboxStateRender->setEnabled(g_settings.group_state_render_used());

    // Debug markers
    QStringList   group_debug_marker_names = g_settings.group_debug_marker_names();
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

    // Nest options

    // Groupbox
    m_pGroupboxNestOptions = ui->groupboxNestOptions;
    m_pGroupboxNestOptions->setTitle(g_settings.groupbox_nest_options_name());
    m_pGroupboxNestOptions->setChecked(g_settings.groupbox_nest_options_stat());
    m_pGroupboxNestOptions->setEnabled(g_settings.groupbox_nest_options_used());

    // Checkboxes
    QStringList group_nest_options_names = g_settings.group_nest_options_names();
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

    // Connect tab
    if(g_settings.tab_page())
    {
        ui->tabWidget->setCurrentIndex(g_settings.tab_page());
    }
    connect(ui->tabWidget, SIGNAL(currentChanged(int)),
                           SLOT(tabCB(int)));


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
    connect(m_pGroupboxNestOptions, SIGNAL(toggled(bool)), // groupbox
                                    SLOT(groupboxCB(bool)));

    for (int i=0; i < nest_options_cnt; i++) // checkboxes in groupbox
    {
        connect(m_pCheckboxNestOptions[i], SIGNAL(stateChanged(int)),
                                           SLOT(checkboxCB(int)));
    }

    // Save initial state
    m_bGroupInitialState = groupState();

} // constructor

vogleditor_QSettingsDialog::~vogleditor_QSettingsDialog()
{
    delete ui;
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

void vogleditor_QSettingsDialog::tabCB(int page)
{
    g_settings.set_tab_page(page);
}

void vogleditor_QSettingsDialog::groupboxCB(bool state)
{
    // update g_settings
    g_settings.set_group_state_render_stat(m_pCheckboxStateRender->isChecked());
    g_settings.set_group_debug_marker_stat(checkboxValues(ui->groupboxDebugMarker));
    g_settings.set_groupbox_nest_options_stat(m_pGroupboxNestOptions->isChecked());
    g_settings.set_group_nest_options_stat(checkboxValues(ui->groupboxNestOptions));

    //update json tab settings page
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

    QVector<bool> bQVector;
    QList<QCheckBox*>::const_iterator iter = groupCheckBoxes.begin();

    while (iter != groupCheckBoxes.end())
    {
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

    bCurrentState << checkboxValues(ui->groupboxDebugMarker);
    bCurrentState << checkboxValues(ui->groupboxNestOptions);

    return bCurrentState;

} // groupState

bool vogleditor_QSettingsDialog::groupOptionsChanged()
{
    return m_bGroupInitialState != groupState();
}

void vogleditor_QSettingsDialog::save(const char* settingsFile)
{
    g_settings.from_string(ui->textEdit->toPlainText().toStdString().c_str());
    g_settings.save(settingsFile);
}

