#ifndef VOGLEDITOR_QSETTINGSDIALOG_H
#define VOGLEDITOR_QSETTINGSDIALOG_H

#include <QDialog>

class QVBoxLayout;
class QScrollArea;
class QGroupBox;
class QCheckBox;

namespace Ui {
class vogleditor_QSettingsDialog;
}

class vogleditor_QSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit vogleditor_QSettingsDialog(QWidget *parent = 0);
    ~vogleditor_QSettingsDialog();

    int groupsDebugMarkersListSize();
    int groupsNestCallsListSize();

    void save(const char* settingsFile);

private slots:
    void checkboxCB(int);

private:
    void clearLayout(QLayout*);

private:
    Ui::vogleditor_QSettingsDialog *ui;

    QVBoxLayout *m_vlayoutTotal;

    QCheckBox   *m_checkboxStateRender; 

    // Debug marker groups groupbox
    QVBoxLayout *m_vlayoutGroupBoxMarkers;
    QGroupBox   *m_groupboxDebugMarkers;
    QCheckBox  **m_checkboxMarkList;

    // NestOptions options groupbox
    QScrollArea *m_scrollareaGroupBoxNestOptions;
    QVBoxLayout *m_vlayoutGroupBoxNestOptions;
    QGroupBox   *m_groupboxNestOptions;
    QVBoxLayout *m_vlayoutNestOptions;
    QWidget     *m_widgetNestOptions;
    QCheckBox  **m_checkboxNestList;

    int m_debugMarkerListSize;
    int m_nestedCallsListSize;

};

#endif // VOGLEDITOR_QSETTINGSDIALOG_H
