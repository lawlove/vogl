#ifndef VOGLEDITOR_SETTINGS_H
#define VOGLEDITOR_SETTINGS_H

#include "vogl_dynamic_string.h"
#include "vogl_json.h"
#include <QStringList>
#include <QVector>

class vogleditor_settings;
extern vogleditor_settings g_settings;

struct vogleditor_setting_struct
{
    int tab_page;

    int window_position_left;
    int window_position_top;
    int window_size_width;
    int window_size_height;
    unsigned int trim_large_trace_prompt_size;

    QString state_render_name;
    bool    state_render_stat;
    bool    state_render_used;

    QStringList   debug_marker_list;
    QVector<bool> debug_marker_stat;
    QVector<bool> debug_marker_used;

    QString groupbox_nest_options_name;
    bool    groupbox_nest_options_stat;
    bool    groupbox_nest_options_used;
    QStringList   nest_options_list;
    QVector<bool> nest_options_stat;
    QVector<bool> nest_options_used;
};

class vogleditor_settings
{
public:
    vogleditor_settings();
    virtual ~vogleditor_settings()
    {
    }

    bool load(const char *settingsFile);
    bool save(const char *settingsFile);

    QString to_string();
    bool from_string(const char *settingsStr);

    int tab_page()
    {
        return m_settings.tab_page;
    }
    void set_tab_page(int page)
    {
        m_settings.tab_page = page;
    }
    int window_position_left()
    {
        return m_settings.window_position_left;
    }
    int window_position_top()
    {
        return m_settings.window_position_top;
    }
    int window_size_width()
    {
        return m_settings.window_size_width;
    }
    int window_size_height()
    {
        return m_settings.window_size_height;
    }
    void set_window_position_left(int window_position_left)
    {
        m_settings.window_position_left = window_position_left;
    }
    void set_window_position_top(int window_position_top)
    {
        m_settings.window_position_top = window_position_top;
    }
    void set_window_size_width(int window_size_width)
    {
        m_settings.window_size_width = window_size_width;
    }
    void set_window_size_height(int window_size_height)
    {
        m_settings.window_size_height = window_size_height;
    }

    unsigned int trim_large_trace_prompt_size()
    {
        return m_settings.trim_large_trace_prompt_size;
    }
    void set_trim_large_trace_prompt_size(unsigned int trim_large_trace_prompt_size)
    {
        m_settings.trim_large_trace_prompt_size = trim_large_trace_prompt_size;
    }

    // State/Render
    QString group_state_render_name()
    {
        return m_settings.state_render_name;
    }
    bool group_state_render_stat()
    {
        return m_settings.state_render_stat;
    }
    void set_group_state_render_stat(bool state_render_stat)
    {
        m_settings.state_render_stat = state_render_stat;
    }
    bool group_state_render_used()
    {
        return m_settings.state_render_used;
    }

    // Debug marker
    QStringList group_debug_marker_names()
    {
        return m_settings.debug_marker_list;
    }
    QVector<bool> group_debug_marker_stat()
    {
        return m_settings.debug_marker_stat;
    }
    void set_group_debug_marker_stat(QVector<bool> debug_marker_stat)
    {
        m_settings.debug_marker_stat = debug_marker_stat;
    }
    QVector<bool> group_debug_marker_used()
    {
        return m_settings.debug_marker_used;
    }
    void set_group_debug_marker_used(QVector<bool> debug_marker_used)
    {
        m_settings.debug_marker_used = debug_marker_used;
    }

    // Nest options

    // Groupbox
    QString groupbox_nest_options_name()
    {
        return m_settings.groupbox_nest_options_name;
    }
    bool groupbox_nest_options_stat()
    {
        return m_settings.groupbox_nest_options_stat;
    }
    void set_groupbox_nest_options_stat(bool b_val)
    {
        m_settings.groupbox_nest_options_stat = b_val;
    }
    bool groupbox_nest_options_used()
    {
        return m_settings.groupbox_nest_options_used;
    }

    // Checkboxes
    QStringList group_nest_options_names()
    {
        return m_settings.nest_options_list;
    }
    QVector<bool> group_nest_options_stat()
    {
        return m_settings.nest_options_stat;
    }
    void set_group_nest_options_stat(QVector<bool> nest_options_stat)
    {
        m_settings.nest_options_stat = nest_options_stat;
    }
    QVector<bool> group_nest_options_used()
    {
        return m_settings.nest_options_used;
    }
    void set_group_nest_options_used(QVector<bool> nest_options_used)
    {
        m_settings.nest_options_used = nest_options_used;
    }

    void update_group_active_lists()
    {
        m_active_debug_marker = active_debug_marker();
        m_active_nest_options = active_nest_options();
    }

    bool is_active_debug_marker (QString str)
    {
        return m_active_debug_marker.contains(str);
    }
    bool is_active_nest_options (QString str)
    {
        return m_settings.groupbox_nest_options_stat
            && m_active_nest_options.contains(str);
    }

private:
    const QStringList active_debug_marker() const
    {
        
        QStringList activeList;
        for (int i=0; i< m_settings.debug_marker_list.count(); i++)
        {
            if (m_settings.debug_marker_stat[i])
            {
                activeList << m_settings.debug_marker_list[i].split("/");
            }
        }
        return activeList;
    }
    const QStringList active_nest_options() const
    {
        
        QStringList activeList;
        for (int i=0; i< m_settings.nest_options_list.count(); i++)
        {
            if (m_settings.nest_options_stat[i])
            {
                activeList << m_settings.nest_options_list[i].split("/");
            }
        }
        return activeList;
    }

private:
    unsigned int m_file_format_version;
    vogleditor_setting_struct m_settings;
    vogleditor_setting_struct m_defaults;

    QStringList m_active_debug_marker;
    QStringList m_active_nest_options;

    vogl::dynamic_string get_settings_path(const char *settingsFilename);
    bool to_json(vogl::json_document &doc);
    bool from_json(const vogl::json_document &doc);
};

#endif // VOGLEDITOR_SETTINGS_H
