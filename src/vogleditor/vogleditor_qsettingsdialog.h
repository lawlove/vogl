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

    int groupsDebugMarkerListSize();
    int groupsNestOptionsListSize();

    void save(const char* settingsFile);

private slots:
    void checkboxCB(int);

private:
    void clearLayout(QLayout*);

private:
    Ui::vogleditor_QSettingsDialog *ui;

    QCheckBox  *m_pCheckboxStateRender; 
    QCheckBox **m_pCheckboxDebugMarker;
    QCheckBox **m_pCheckboxNestOptions;

    int m_debugMarkerListSize;
    int m_nestOptionsListSize;
};

#endif // VOGLEDITOR_QSETTINGSDIALOG_H
