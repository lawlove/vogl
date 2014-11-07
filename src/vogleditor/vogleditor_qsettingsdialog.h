#ifndef VOGLEDITOR_QSETTINGSDIALOG_H
#define VOGLEDITOR_QSETTINGSDIALOG_H

#include <QDialog>
#include <QVector>

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

    bool groupOptionsChanged();
    void save(const char* settingsFile);

private slots:
    void tabCB(int);
    void checkboxCB(int);
    void groupboxCB(bool);
    void radiobuttonNameCB(bool);
    void radiobuttonOmitCB(bool);

private:
    QVector<bool> checkboxValues(QGroupBox *);
    QVector<bool> groupState();
    void updateTextTab();
    void clearLayout(QLayout*);
    void setEnableDebugMarkerOptions();
    void enableDebugMarkerOptions(bool);

private:
    Ui::vogleditor_QSettingsDialog *ui;

    QVector<bool> m_bGroupInitialState;
};

#endif // VOGLEDITOR_QSETTINGSDIALOG_H
