#ifndef VOGLEDITOR_SETTINGS_H
#define VOGLEDITOR_SETTINGS_H

//#include "vogleditor_settingsgroup.h"
#include "vogl_dynamic_string.h"
#include "vogl_json.h"
#include <QStringList>
#include <QVector>

class vogleditor_settings;
extern vogleditor_settings g_settings;

struct vogleditor_setting_struct
{
    int window_position_left;
    int window_position_top;
    int window_size_width;
    int window_size_height;
    unsigned int trim_large_trace_prompt_size;

#ifdef LLL
/*  bool groups_state_render;
    bool groups_push_pop_markers;
    bool groups_nested_calls; */
#endif // LLL

    QString state_render_name;
    bool    state_render_stat;
    bool    state_render_used;

    QStringList   debug_marker_list;
    QVector<bool> debug_marker_stat;
    QVector<bool> debug_marker_used;

    QStringList   nest_options_list;
    QVector<bool> nest_options_stat;
    QVector<bool> nest_options_used;

#ifdef LLL
/*  QStringList   group_list;
    QVector<bool> group_settings;
    QVector<bool> group_enabled; */
#endif // LLL
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

#ifdef LLL
    // Groups 1
/*  bool groups_state_render()
    {
        return m_settings.groups_state_render;
    }
    bool groups_push_pop_markers()
    {
        return m_settings.groups_push_pop_markers;
    }
    bool groups_nested_calls()
    {
        return m_settings.groups_nested_calls;
    }
    void set_groups_state_render(bool groups_state_render)
    {
        m_settings.groups_state_render = groups_state_render;
    }
    void set_groups_push_pop_markers(bool groups_push_pop_markers)
    {
        m_settings.groups_push_pop_markers = groups_push_pop_markers;
    }
    void set_groups_nested_calls(bool groups_nested_calls)
    {
        m_settings.groups_nested_calls = groups_nested_calls;
    }

    // Groups 2
    QStringList group_names()
    {
        return m_settings.group_list;
    }
    void set_group_names(QStringList groupnames)
    {
        m_settings.group_list = groupnames;
    }

    QVector<bool> group_settings()
    {
        return m_settings.group_settings;
    }
    void set_group_settings(QVector<bool> groupsettings)
    {
        m_settings.group_settings = groupsettings;
    } */
#endif //LLL

    // Groups 3
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

private:
    unsigned int m_file_format_version;
    vogleditor_setting_struct m_settings;
    vogleditor_setting_struct m_defaults;

    vogl::dynamic_string get_settings_path(const char *settingsFilename);
    bool to_json(vogl::json_document &doc);
    bool from_json(const vogl::json_document &doc);
};

#endif // VOGLEDITOR_SETTINGS_H
