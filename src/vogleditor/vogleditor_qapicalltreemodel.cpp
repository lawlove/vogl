/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/

#include <QColor>
#include <QFont>
#include <QLocale>

#include "vogleditor_qapicalltreemodel.h"

#include "vogl_common.h"
#include "vogl_trace_file_reader.h"
#include "vogl_trace_packet.h"
#include "vogl_trace_stream_types.h"
#include "vogleditor_gl_state_snapshot.h"
#include "vogleditor_frameitem.h"
#include "vogleditor_groupitem.h"
#include "vogleditor_apicalltreeitem.h"
#include "vogleditor_output.h"

vogleditor_QApiCallTreeModel::vogleditor_QApiCallTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    m_rootItem = vogl_new(vogleditor_apiCallTreeItem, this);
}

vogleditor_QApiCallTreeModel::~vogleditor_QApiCallTreeModel()
{
    if (m_rootItem != NULL)
    {
        vogl_delete(m_rootItem);
        m_rootItem = NULL;
    }

    if (m_pTrace_ctypes != NULL)
    {
        vogl_delete(m_pTrace_ctypes);
        m_pTrace_ctypes = NULL;
    }

    m_itemList.clear();
}

bool vogleditor_QApiCallTreeModel::init(vogl_trace_file_reader* pTrace_reader)
{
    const vogl_trace_stream_start_of_file_packet &sof_packet = pTrace_reader->get_sof_packet();
    VOGL_NOTE_UNUSED(sof_packet);

    uint64_t total_swaps = 0;
    bool found_eof_packet = false;

    // make a PendingFrame node to hold the api calls
    // this will remain in the pending state until the first
    // api call is seen, then it will be made the CurFrame and
    // appended to the parent
    vogleditor_frameItem *pCurFrame = NULL;
    vogleditor_groupItem *pCurGroup = NULL;
    vogleditor_apiCallTreeItem* parentRoot = m_rootItem;
    vogleditor_apiCallTreeItem* pCurParent = parentRoot;

    // Make a PendingSnapshot that may or may not be populated when reading the trace.
    // This snapshot will be assigned to the next API call that occurs.
    vogleditor_gl_state_snapshot* pPendingSnapshot = NULL;

    m_pTrace_ctypes = vogl_new(vogl_ctypes);

    if (m_pTrace_ctypes == NULL)
    {
        return false;
    }

    m_pTrace_ctypes->init(pTrace_reader->get_sof_packet().m_pointer_sizes);
    int n=0;
    bool bDoPop=true;
    for ( ; ; )
    {
        vogl_trace_file_reader::trace_file_reader_status_t read_status = pTrace_reader->read_next_packet();

        if ((read_status != vogl_trace_file_reader::cOK) && (read_status != vogl_trace_file_reader::cEOF))
        {
            vogleditor_output_error("Failed reading from trace file!");
            return false;
        }

        if (read_status == vogl_trace_file_reader::cEOF)
        {
            vogl_printf("At trace file EOF on swap %" PRIu64 "\n", total_swaps);
            return false;
        }

        const vogl::vector<uint8_t> &packet_buf = pTrace_reader->get_packet_buf(); VOGL_NOTE_UNUSED(packet_buf);

        const vogl_trace_stream_packet_base &base_packet = pTrace_reader->get_base_packet(); VOGL_NOTE_UNUSED(base_packet);
        const vogl_trace_gl_entrypoint_packet *pGL_packet = NULL;

        if (pTrace_reader->get_packet_type() == cTSPTGLEntrypoint)
        {
            vogl_trace_packet* pTrace_packet = vogl_new(vogl_trace_packet, m_pTrace_ctypes);

            if (!pTrace_packet->deserialize(pTrace_reader->get_packet_buf().get_ptr(), pTrace_reader->get_packet_buf().size(), false))
            {
                vogleditor_output_error("Failed parsing GL entrypoint packet.");
                return false;
            }

            if (!pTrace_packet->check())
            {
                vogleditor_output_error("GL entrypoint packet failed"
                " consistency check. Please make sure the trace was made with"
                " the most recent version of VOGL.");
                return false;
            }

            pGL_packet = &pTrace_reader->get_packet<vogl_trace_gl_entrypoint_packet>();
            gl_entrypoint_id_t entrypoint_id = static_cast<gl_entrypoint_id_t>(pGL_packet->m_entrypoint_id);

            if (entrypoint_id == VOGL_ENTRYPOINT_glInternalTraceCommandRAD)
            {
                // Check if this is a state snapshot.
                // This is entirely optional since the client is designed to dynamically get new snapshots
                // if they don't exist.
                GLuint cmd = pTrace_packet->get_param_value<GLuint>(0);
                GLuint size = pTrace_packet->get_param_value<GLuint>(1); VOGL_NOTE_UNUSED(size);

                if (cmd == cITCRKeyValueMap)
                {
                    key_value_map &kvm = pTrace_packet->get_key_value_map();

                    dynamic_string cmd_type(kvm.get_string("command_type"));
                    if (cmd_type == "state_snapshot")
                    {
                        dynamic_string id(kvm.get_string("binary_id"));
                        if (id.is_empty())
                        {
                            vogl_warning_printf("Missing binary_id field in glInternalTraceCommandRAD key_value_map command type: \"%s\"\n", cmd_type.get_ptr());
                            continue;
                        }

                        uint8_vec snapshot_data;
                        {
                            timed_scope ts("get_multi_blob_manager().get");
                            if (!pTrace_reader->get_multi_blob_manager().get(id, snapshot_data) || (snapshot_data.is_empty()))
                            {
                                vogl_warning_printf("Failed reading snapshot blob data \"%s\"!\n", id.get_ptr());
                                continue;
                            }
                        }

                        json_document doc;
                        {
                            timed_scope ts("doc.binary_deserialize");
                            if (!doc.binary_deserialize(snapshot_data) || (!doc.get_root()))
                            {
                                vogl_warning_printf("Failed deserializing JSON snapshot blob data \"%s\"!\n", id.get_ptr());
                                continue;
                            }
                        }

                        vogl_gl_state_snapshot* pGLSnapshot = vogl_new(vogl_gl_state_snapshot);
                        pPendingSnapshot = vogl_new(vogleditor_gl_state_snapshot, pGLSnapshot);

                        timed_scope ts("pPendingSnapshot->deserialize");
                        if (!pPendingSnapshot->get_snapshot()->deserialize(*doc.get_root(), pTrace_reader->get_multi_blob_manager(), m_pTrace_ctypes))
                        {
                            vogl_delete(pPendingSnapshot);
                            pPendingSnapshot = NULL;

                            vogl_warning_printf("Failed deserializing snapshot blob data \"%s\"!\n", id.get_ptr());
                        }
                    }
                }

                continue;
            }

            const gl_entrypoint_desc_t &entrypoint_desc = g_vogl_entrypoint_descs[entrypoint_id];

            QString funcCall = entrypoint_desc.m_pName;

            // format parameters
            funcCall.append("( ");
            dynamic_string paramStr;
            for (uint param_index = 0; param_index < pTrace_packet->total_params(); param_index++)
            {
                if (param_index != 0)
                    funcCall.append(", ");

                paramStr.clear();
                pTrace_packet->pretty_print_param(paramStr, param_index, false);

                funcCall.append(paramStr.c_str());
            }
            funcCall.append(" )");

            if (pTrace_packet->has_return_value())
            {
                funcCall.append(" = ");
                paramStr.clear();
                pTrace_packet->pretty_print_return_value(paramStr, false);
                funcCall.append(paramStr.c_str());
            }

// LLL-------------------------------------------------------------------
// Check if a new frame node needs to be created
// LLL-------------------------------------------------------------------
            // if we don't have a current frame, make a new frame node
            // and append it to the parentRoot
            if (pCurFrame == NULL)
            {
                pCurFrame = vogl_new(vogleditor_frameItem, total_swaps);
                vogleditor_apiCallTreeItem* pNewFrameNode = vogl_new(vogleditor_apiCallTreeItem, pCurFrame, pCurParent);
                // LLL pCurParent should be parentRoot when adding frame
                // LLL i.e., "parentRoot->appendChild(pNewFrameNode);"
                pCurParent->appendChild(pNewFrameNode);
                m_itemList.append(pNewFrameNode);

                if (pPendingSnapshot != NULL)
                {
                    pCurFrame->set_snapshot(pPendingSnapshot);
                    pPendingSnapshot = NULL;
                }

                // update current parent
                pCurParent = pNewFrameNode;
            } // pCurFrame == NULL
// LLL-------------------------------------------------------------------
// Start processing next entity after possible new frame node created
// LLL-------------------------------------------------------------------
            // Frame treenodes should only consist of (child) group-type
            // treenodes, e.g., group or nesting nodes(glPush/PopGroups)

// LLL-------------------------------------------------------------------
// glPushDebugGroup
// LLL-------------------------------------------------------------------
            // ---- Is this a start_nested_entrypoint (glPushDebugGroup)?
            //if (vogl_is_start_nested_entrypoint(entrypoint_id))
            if (entrypoint_id == VOGL_ENTRYPOINT_glPushDebugGroup)
            {
                // ---- Is parent a state/render group and grandparent a frame?
                //      (...and does grandparent need to be checked for null?)
                // ---- 
                if (pCurParent->isGroup())
                {
                    // Special case, make frame the parent
                    if (pCurParent->parent()->isFrame())
                    {
                        // Make 1st level glPushDebugGroup a frame child
                        pCurParent = pCurParent->parent();  // (set parent back toframe)
                    }
                    else // check for sequential glPushDebugGroup apicalls
                    {
                        // If the previous apicall was a glPushDebugGroup, then
                        // a State/Render group had been created as its first
                        // child and set as, and is now, the new pCurParent. 
                        //
                        // For a sequential glPushdDebugGroup apicall, undo that
                        // operation so this one will become the first child
                        // instead (i.e., replace the previous [group] first
                        // child with this [pseudo-group] apiCall
                        //
                        // Get grandparent entrypoint_id
                        if (pCurParent->parent()->isApiCall())
                        {
                            uint16_t id = pCurParent->parent()->apiCallItem()->getGLPacket()->m_entrypoint_id;
                            gl_entrypoint_id_t grandpa_entrypoint_id = static_cast<gl_entrypoint_id_t>(id);

                            // Check if grandpa is glPushDebugGroup
                            if (grandpa_entrypoint_id == VOGL_ENTRYPOINT_glPushDebugGroup)
                            {
                                if (pCurParent->childCount() == 0)
                                {
                                   delete_group(pCurFrame, pCurParent);
                                }
                                else
                                {
                                    pCurParent = pCurParent->parent();
                                }
                            }
                        }
                    }
                }
            }
// LLL-------------------------------------------------------------------
// glPopDebugGroup
// LLL-------------------------------------------------------------------
            //else if (vogl_is_end_nested_entrypoint(entrypoint_id))
            else if (entrypoint_id == VOGL_ENTRYPOINT_glPopDebugGroup)
            {
                // if parent is group (should be and grandparent should be
                // glPushDebugGroup), close it; set curent parent to
                // grandparent (group's parent) which will get popped
                // after glPopDebugGroup tree node is processed
                if (pCurParent->isGroup())
                {
                    pCurParent = pCurParent->parent();
                }
            }
            // ---- not a start_ or end_nested_entrypoint (glPush/PopDebugGroup)
// LLL-------------------------------------------------------------------
// glBegin
// LLL-------------------------------------------------------------------
            //else
            else if ((entrypoint_id == VOGL_ENTRYPOINT_glBegin))
            {
                if (pCurParent->isGroup()) // state/render group?
                {
                    // delete empty group and make grandparent the new parent
                    if (pCurParent->childCount() == 0)
                    {
                        delete_group(pCurFrame, pCurParent);
                    }
                    //  Start a new (render) group if not already in one
                    //  (Will be set to "Render" on glEnd)
                    // ** Need to think about a more unique name.. or **
                    // ** contains "Render" but not "group" or double **
                    // ** quotes ('"')                                **
                    if (!(pCurParent->columnData(VOGL_ACTC_APICALL, Qt::DisplayRole)).toString().contains("Render"))
                    {
                        // close this group by setting parent as curParent.
                        // Add new group to parent and make group curParent.
                        pCurParent = pCurParent->parent();
                        pCurParent = create_new_group(pCurFrame, pCurGroup, pCurParent);
                    }
                }
            }
#ifdef LLL
            else if ((entrypoint_id == VOGL_ENTRYPOINT_glEnd))
            {
            }
#endif //LLL
// LLL-------------------------------------------------------------------
// None of the above
// LLL-------------------------------------------------------------------
            else // regular apiCall
            {
                // ---- If at frame level, start a new state/render group
                if (pCurParent->isFrame())
                {
                    // ---- Start a new group and make it the current parent
                    pCurParent = create_new_group(pCurFrame, pCurGroup, pCurParent);
                }
                // This apicall is not glBegin (would have been caught in above
                // if-block). But if prev apicall was glEnd then we're done
                // rendering a sequence of one or more glBegin/End blocks. The
                // current parent is a "Render" group so close it and start a
                // new State/Render group under the grandparent. (Although if
                // this is a "Draw" call we should skip and let the apicall
                // post-process logic close it)
                else if (!m_itemList.isEmpty())
                {
                    if (m_itemList.last()->isApiCall())
                    {
                        uint16_t id = m_itemList.last()->apiCallItem()->getGLPacket()->m_entrypoint_id;
                        gl_entrypoint_id_t prevApiCallId = static_cast<gl_entrypoint_id_t>(id);

                        if (prevApiCallId == VOGL_ENTRYPOINT_glEnd)
                        {
                            pCurParent = pCurParent->parent();
                            pCurParent = create_new_group(pCurFrame, pCurGroup, pCurParent);
                        }
                    }
                }
            } // not a start_ or end_nested_entrypoint

// LLL-------------------------------------------------------------------
// Process apiCall: create apiCall treenode and append to current parent
// LLL-------------------------------------------------------------------
            vogleditor_apiCallItem* pCallItem = NULL;
            vogleditor_apiCallTreeItem* item = NULL;
        if (entrypoint_id != VOGL_ENTRYPOINT_glPopDebugGroup) // skip these
        {
            // make item for the api call
            pCallItem = vogl_new(vogleditor_apiCallItem, pCurFrame, pTrace_packet, *pGL_packet);
            pCurFrame->appendCall(pCallItem);

            if (pPendingSnapshot != NULL)
            {
                pCallItem->set_snapshot(pPendingSnapshot);
                pPendingSnapshot = NULL;
            }

            // make node for the api call
            item = vogl_new(vogleditor_apiCallTreeItem, funcCall, pCallItem, pCurParent);
            pCurParent->appendChild(item);
            m_itemList.append(item);
        }

// LLL-------------------------------------------------------------------
// Check for apicall terminators
// LLL-------------------------------------------------------------------
// swap:
// LLL-------------------------------------------------------------------
            if (vogl_is_swap_buffers_entrypoint(entrypoint_id))
            {
                total_swaps++;

                // TODO: close up any groups first

                // reset the CurParent back to the original parent so that the next frame will be at the root level
                pCurParent = parentRoot;

                // reset the CurFrame so that a new frame node will be created on the next api call
                pCurFrame = NULL;
            }
// LLL-------------------------------------------------------------------
// glBegin:
// LLL-------------------------------------------------------------------
            else if (entrypoint_id == VOGL_ENTRYPOINT_glBegin)
            {
                // close previous group as state and start a new group
                // that will be a render group - add this as child and
                // new parent
                //
                // On glEnd, end the glBegin nest
                //
                //
                pCurParent = item;
            }
// LLL-------------------------------------------------------------------
// glEnd:
// LLL-------------------------------------------------------------------
            else if (entrypoint_id == VOGL_ENTRYPOINT_glEnd)
            {
                if (!pCurParent->isFrame())
                    pCurParent = pCurParent->parent();
            }
// LLL-------------------------------------------------------------------
// glPushDebugGroup:
// LLL-------------------------------------------------------------------
            else if (entrypoint_id == VOGL_ENTRYPOINT_glPushDebugGroup)
            {
                // Append a new state/render group child to glPushDebugGroup
                // and make it new parent
                pCurParent = create_new_group(pCurFrame, pCurGroup, item);
            }
// LLL-------------------------------------------------------------------
// glPopDebugGroup:
// LLL-------------------------------------------------------------------
            else if (entrypoint_id == VOGL_ENTRYPOINT_glPopDebugGroup)
            {
                // Don't pop past a frame [e.g., if this is an unpaired "glEnd"]
                if (!pCurParent->isFrame())
                {
                    // move up to glPush (past any groups)
                    while (pCurParent->isGroup())
                    {
                        pCurParent = pCurParent->parent();
                    }

                    // Parse out parent glPushDebugGroup messsage
                    QString apiCall = (pCurParent->columnData(VOGL_ACTC_APICALL, Qt::DisplayRole)).toString();
                    QString sec, name;
                    int start = 1;
                    while (!(sec=apiCall.section('\'', start, start)).isEmpty())
                    {
                        name.append(sec);
                        start +=2;
                    }

                    // Rename parent tree node (glPushDebugGroup apiCall)
                    pCurParent->setCallTreeApiCallColumnData(QVariant("\"" + name + "\"" + " group"));

                    // End glPushDebugGroup parent set and move curParent back
                    // up to glPushDebugGroup's parent
                    pCurParent = pCurParent->parent();

                    // start a new group
                    //pCurParent = create_new_group(pCurFrame, pCurGroup, pCurParent);
                }
            }
// LLL-------------------------------------------------------------------
// Draw:
// LLL-------------------------------------------------------------------
// >>LLL Close group on draw and start new group
            if (vogl_is_draw_entrypoint(entrypoint_id))
            {
                if (pCurParent->isFrame()) // (should never be frame tho)
                {
                    // Start a new group
                    pCurParent = create_new_group(pCurFrame, pCurGroup, pCurParent);
                }
                // not a frame, close group and set to "Render"
                else if (pCurParent->isGroup())
                {
                    if ((pCurParent->columnData(VOGL_ACTC_APICALL, Qt::DisplayRole)).toString().contains("Render"))
                    {
                        // Stop this group and move back to prev parent
                        //pCurParent = pCurParent->parent();
                    }
                    else
                    {
                        // Set group node column data
                        pCurParent->setCallTreeApiCallColumnData("Render" + QLocale().toString(n++));
                    }
                }
            }
// <<LLL
        } // if cTSPTGLEntrypoint

        if (pTrace_reader->get_packet_type() == cTSPTEOF)
        {
            found_eof_packet = true;
            vogl_printf("Found trace file EOF packet on swap %" PRIu64 "\n", total_swaps);
            break;
        }
    }

    return found_eof_packet;
}

void vogleditor_QApiCallTreeModel::delete_apiCall(vogleditor_frameItem  *pCurFrameObj,
                                                  vogleditor_apiCallTreeItem *&pParentNode)
{
    // Remove parent frame's last appended group item (vogleditor_groupItem)
    // and delete it.
    vogl_delete(pCurFrameObj->popApiCall());

    // Clear pCurParent (group treenode) from both parent (glPushDebugGroup)
    // and m_itemList
    pParentNode->parent()->popChild();
    m_itemList.removeLast();

    // Set pCurParent to pCurParent->parent()
    vogleditor_apiCallTreeItem *pOldParent = pParentNode;
    pParentNode = pParentNode->parent();

    // Delete old pCurParent
    vogl_delete(pOldParent);
}

void vogleditor_QApiCallTreeModel::delete_group(vogleditor_frameItem  *pCurFrameObj,
                                                vogleditor_apiCallTreeItem *&pParentNode)
{
    // Assumptions 
    // ------------
    // grandparent (pCurParent->parent() is a glPushDebugGroup apiCall treenode
    //
    // pCurrentParent is a group (State/Render) treenode
    //
    // pCurParent treenode's corresponding group item object was the last added
    // group item added to the frame's group item list

    // Remove parent frame's last appended group item (vogleditor_groupItem)
    // and delete it.
    vogl_delete(pCurFrameObj->popGroup());

    // Clear pCurParent (group treenode) from both parent (glPushDebugGroup)
    // and m_itemList
    pParentNode->parent()->popChild();
    m_itemList.removeLast();

    // Set pCurParent to pCurParent->parent()
    vogleditor_apiCallTreeItem *pOldParent = pParentNode;
    pParentNode = pParentNode->parent();

    // Delete old pCurParent
    vogl_delete(pOldParent);
}

vogleditor_apiCallTreeItem *vogleditor_QApiCallTreeModel::create_new_group(vogleditor_frameItem  *pCurFrameObj,
                                                                           vogleditor_groupItem *&pCurGroupObj,
                                                                           vogleditor_apiCallTreeItem *pParentNode)
{
// Description:
//    1) create group
//    2) add group to (parent)

    //LLL Make a new group item ... unused as yet
    pCurGroupObj = vogl_new(vogleditor_groupItem, pCurFrameObj);
    pCurFrameObj->appendGroup(pCurGroupObj); // (move this to constructor?)

    //LLL Make a new group node and insert into tree
    vogleditor_apiCallTreeItem *pNewGroupNode = vogl_new(vogleditor_apiCallTreeItem, pCurGroupObj, pParentNode);
    pParentNode->appendChild(pNewGroupNode);
    m_itemList.append(pNewGroupNode);
    return pNewGroupNode;
}

QModelIndex vogleditor_QApiCallTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    vogleditor_apiCallTreeItem *parentItem;

    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = static_cast<vogleditor_apiCallTreeItem*>(parent.internalPointer());

    vogleditor_apiCallTreeItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

QModelIndex vogleditor_QApiCallTreeModel::indexOf(const vogleditor_apiCallTreeItem* pItem) const
{
    if (pItem != NULL)
        return createIndex(pItem->row(), VOGL_ACTC_APICALL, (void*)pItem);
    else
        return QModelIndex();
}

QModelIndex vogleditor_QApiCallTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    vogleditor_apiCallTreeItem* childItem = static_cast<vogleditor_apiCallTreeItem*>(index.internalPointer());
    vogleditor_apiCallTreeItem* parentItem = childItem->parent();

    if (parentItem == m_rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), VOGL_ACTC_APICALL, parentItem);
}

int vogleditor_QApiCallTreeModel::rowCount(const QModelIndex &parent) const
{
    vogleditor_apiCallTreeItem* parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = static_cast<vogleditor_apiCallTreeItem*>(parent.internalPointer());

    return parentItem->childCount();
}

int vogleditor_QApiCallTreeModel::columnCount(const QModelIndex &parent) const
{
    VOGL_NOTE_UNUSED(parent);
    return VOGL_MAX_ACTC;
}

QVariant vogleditor_QApiCallTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    vogleditor_apiCallTreeItem* pItem = static_cast<vogleditor_apiCallTreeItem*>(index.internalPointer());

    if (pItem == NULL)
    {
        return QVariant();
    }

    // make draw call rows appear in bold
    if (role == Qt::FontRole && pItem->apiCallItem() != NULL && vogl_is_draw_entrypoint((gl_entrypoint_id_t)pItem->apiCallItem()->getGLPacket()->m_entrypoint_id))
    {
        QFont font;
        font.setBold(true);
        return font;
    }

    // highlight the API call cell if it has a substring which matches the searchString
    if (role == Qt::BackgroundRole && index.column() == VOGL_ACTC_APICALL)
    {
        if (!m_searchString.isEmpty())
        {
            QVariant data = pItem->columnData(VOGL_ACTC_APICALL, Qt::DisplayRole);
            QString string = data.toString();
            if (string.contains(m_searchString, Qt::CaseInsensitive))
            {
                return QColor(Qt::yellow);
            }
        }
    }

    return pItem->columnData(index.column(), role);
}

Qt::ItemFlags vogleditor_QApiCallTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant vogleditor_QApiCallTreeModel::headerData(int section, Qt::Orientation orientation,
                                                  int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return m_rootItem->columnData(section, role);

    return QVariant();
}

void vogleditor_QApiCallTreeModel::set_highlight_search_string(const QString searchString)
{
    m_searchString = searchString;
}

QModelIndex vogleditor_QApiCallTreeModel::find_prev_search_result(vogleditor_apiCallTreeItem* start, const QString searchText)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    if (start != NULL)
    {
        if (iter.findNext(start) == false)
        {
            // the object wasn't found in the list, so return a default (invalid) item
            return QModelIndex();
        }

        // need to back up past the current item
        iter.previous();
    }
    else
    {
        // set the iterator to the back so that searching starts from the end of the list
        iter.toBack();
    }

    // now the iterator is pointing to the desired start object in the list,
    // continually check the prev item and find one with a snapshot
    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasPrevious())
    {
        vogleditor_apiCallTreeItem* pItem = iter.peekPrevious();
        QVariant data = pItem->columnData(VOGL_ACTC_APICALL, Qt::DisplayRole);
        QString string = data.toString();
        if (string.contains(searchText, Qt::CaseInsensitive))
        {
            pFound = pItem;
            break;
        }

        iter.previous();
    }

    return indexOf(pFound);
}

QModelIndex vogleditor_QApiCallTreeModel::find_next_search_result(vogleditor_apiCallTreeItem* start, const QString searchText)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    if (start != NULL)
    {
        if (iter.findNext(start) == false)
        {
            // the object wasn't found in the list, so return a default (invalid) item
            return QModelIndex();
        }
    }

    // now the iterator is pointing to the desired start object in the list,
    // continually check the next item and find one with a snapshot
    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasNext())
    {
        vogleditor_apiCallTreeItem* pItem = iter.peekNext();
        QVariant data = pItem->columnData(VOGL_ACTC_APICALL, Qt::DisplayRole);
        QString string = data.toString();
        if (string.contains(searchText, Qt::CaseInsensitive))
        {
            pFound = pItem;
            break;
        }

        iter.next();
    }

    return indexOf(pFound);
}

vogleditor_apiCallTreeItem* vogleditor_QApiCallTreeModel::find_prev_snapshot(vogleditor_apiCallTreeItem* start)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    if (start != NULL)
    {
        if (iter.findNext(start) == false)
        {
            // the object wasn't found in the list
            return NULL;
        }

        // need to back up past the current item
        iter.previous();
    }
    else
    {
        // set the iterator to the back so that searching starts from the end of the list
        iter.toBack();
    }

    // now the iterator is pointing to the desired start object in the list,
    // continually check the prev item and find one with a snapshot
    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasPrevious())
    {
        if (iter.peekPrevious()->has_snapshot())
        {
            pFound = iter.peekPrevious();
            break;
        }

        iter.previous();
    }

    return pFound;
}

vogleditor_apiCallTreeItem* vogleditor_QApiCallTreeModel::find_next_snapshot(vogleditor_apiCallTreeItem* start)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    // if start is NULL, then search will begin from top, otherwise it will begin from the start item and search onwards
    if (start != NULL)
    {
        if (iter.findNext(start) == false)
        {
            // the object wasn't found in the list
            return NULL;
        }
    }

    // now the iterator is pointing to the desired start object in the list,
    // continually check the next item and find one with a snapshot
    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasNext())
    {
        if (iter.peekNext()->has_snapshot())
        {
            pFound = iter.peekNext();
            break;
        }

        iter.next();
    }

    return pFound;
}


vogleditor_apiCallTreeItem *vogleditor_QApiCallTreeModel::find_prev_drawcall(vogleditor_apiCallTreeItem* start)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    if (start != NULL)
    {
        if (iter.findNext(start) == false)
        {
            // the object wasn't found in the list
            return NULL;
        }

        // need to back up past the current item
        iter.previous();
    }
    else
    {
        // set the iterator to the back so that searching starts from the end of the list
        iter.toBack();
    }

    // now the iterator is pointing to the desired start object in the list,
    // continually check the prev item and find one with a snapshot
    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasPrevious())
    {
        vogleditor_apiCallTreeItem* pItem = iter.peekPrevious();
        if (pItem->apiCallItem() != NULL)
        {
            gl_entrypoint_id_t entrypointId = static_cast<gl_entrypoint_id_t>(pItem->apiCallItem()->getGLPacket()->m_entrypoint_id);
            if (vogl_is_draw_entrypoint(entrypointId) ||
                    vogl_is_clear_entrypoint(entrypointId) ||
                    (entrypointId == VOGL_ENTRYPOINT_glBitmap) ||
                    (entrypointId == VOGL_ENTRYPOINT_glEnd))
            {
                pFound = iter.peekPrevious();
                break;
            }
        }

        iter.previous();
    }

    return pFound;
}

vogleditor_apiCallTreeItem *vogleditor_QApiCallTreeModel::find_next_drawcall(vogleditor_apiCallTreeItem* start)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    if (iter.findNext(start) == false)
    {
        // the object wasn't found in the list
        return NULL;
    }

    // now the iterator is pointing to the desired start object in the list,
    // continually check the next item and find one with a snapshot
    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasNext())
    {
        vogleditor_apiCallTreeItem* pItem = iter.peekNext();
        if (pItem->apiCallItem() != NULL)
        {
            gl_entrypoint_id_t entrypointId = static_cast<gl_entrypoint_id_t>(pItem->apiCallItem()->getGLPacket()->m_entrypoint_id);
            if (vogl_is_draw_entrypoint(entrypointId) ||
                    vogl_is_clear_entrypoint(entrypointId) ||
                    (entrypointId == VOGL_ENTRYPOINT_glBitmap) ||
                    (entrypointId == VOGL_ENTRYPOINT_glEnd))
            {
                pFound = iter.peekNext();
                break;
            }
        }

        iter.next();
    }

    return pFound;
}

vogleditor_apiCallTreeItem* vogleditor_QApiCallTreeModel::find_call_number(unsigned int callNumber)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasNext())
    {
        vogleditor_apiCallTreeItem* pItem = iter.peekNext();
        if (pItem->apiCallItem() != NULL)
        {
            if (pItem->apiCallItem()->globalCallIndex() == callNumber)
            {
                pFound = iter.peekNext();
                break;
            }
        }

        iter.next();
    }

    return pFound;
}

vogleditor_apiCallTreeItem* vogleditor_QApiCallTreeModel::find_frame_number(unsigned int frameNumber)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasNext())
    {
        vogleditor_apiCallTreeItem* pItem = iter.peekNext();
        if (pItem->frameItem() != NULL)
        {
            if (pItem->frameItem()->frameNumber() == frameNumber)
            {
                pFound = iter.peekNext();
                break;
            }
        }

        iter.next();
    }

    return pFound;
}
